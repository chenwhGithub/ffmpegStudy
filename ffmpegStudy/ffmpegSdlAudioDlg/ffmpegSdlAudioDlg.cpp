// C:\code\H264\ffmpegStudy\ffmpegStudy\ffmpegSdlAudioDlg\ffmpegSdlAudioDlg.cpp : implementation file
//

#include "../pch.h"
#include "../ffmpegStudy.h"
#include "ffmpegSdlAudioDlg.h"
#include "afxdialogex.h"

#ifdef __cplusplus
extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/codec.h"
#include "libavcodec/avcodec.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}
#endif

#include "SDL.h"

#define MAX_AUDIO_FRAME_SIZE 19200

static Uint8 *audio_chunk;
static Uint32 audio_len;
static Uint8 *audio_pos;

// 音频设备需要更多数据的时候会调用该回调函数
void read_audio_data(void *udata, Uint8 *stream, int len)
{
    SDL_memset(stream, 0, len);
    if (audio_len == 0)
        return;
    len = (len > audio_len ? audio_len : len);

    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
}


// CffmpegSdlAudioDlg dialog

IMPLEMENT_DYNAMIC(CffmpegSdlAudioDlg, CDialogEx)

CffmpegSdlAudioDlg::CffmpegSdlAudioDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_FFMPEGSDL_AUDIO, pParent)
{

}

CffmpegSdlAudioDlg::~CffmpegSdlAudioDlg()
{
}

void CffmpegSdlAudioDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MFCEDITBROWSE, m_browse);
}


BEGIN_MESSAGE_MAP(CffmpegSdlAudioDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_PLAY_AUDIO, &CffmpegSdlAudioDlg::OnClickedButtonPlayAudio)
END_MESSAGE_MAP()


// CffmpegSdlAudioDlg message handlers


void CffmpegSdlAudioDlg::OnClickedButtonPlayAudio()
{
    // TODO: Add your control notification handler code here
    AVFormatContext *pFormatCtx;    // 封装格式上下文结构体，也是统领全局的结构体，保存了视频文件封装格式的相关信息
    AVCodecContext *pCodecCtx;      // 编码器上下文结构体，保存了视频音频编解码相关信息
    AVCodec *pCodec;                // 每种视频音频编解码器对应一个该结构体
    AVFrame *pFrame;                // 存储一帧解码后像素或采样数据
    AVPacket *packet;               // 存储一帧压缩编码数据
    unsigned char *out_buffer;      // 存储图像数据
    int i, audioindex;

    int64_t in_channel_layout;
    struct SwrContext *au_convert_ctx;

    CString strFileName;
    char * chFileName;
    m_browse.GetWindowTextW(strFileName);
    if (strFileName.IsEmpty())
    {
        return;
    }
    USES_CONVERSION;
    chFileName = W2A(strFileName);

    // av_register_all();
    avformat_network_init();        // 网络组件的全局初始化
    pFormatCtx = avformat_alloc_context();  // 分配 AVFormatContext

    // 打开输入的音视频文件
    if (avformat_open_input(&pFormatCtx, chFileName, NULL, NULL) != 0) {
        printf("Couldn't open input stream.\n");
        return;
    }

    // 读取视音频文件信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        printf("Couldn't find stream information.\n");
        return;
    }
    /*
    // 每个视频文件中有多个流(视频流、音频流、字幕流等，而且可有多个)，循环遍历找到音频流
    audioindex = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioindex = i;
            break;
        }
    }
    */
    audioindex = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audioindex == -1) {
        printf("Didn't find a audio stream.\n");
        return;
    }

    // 保存音频流中的 AVCodecContext
    pCodecCtx = avcodec_alloc_context3(NULL);
    if (pCodecCtx == NULL)
    {
        printf("Could not allocate AVCodecContext\n");
        return;
    }
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[audioindex]->codecpar);

    // 查找解码器
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        printf("Codec not found.\n");
        return;
    }

    // 打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec.\n");
        return;
    }

    // 创建 AVFrame，用来存放解码后的一帧的数据
    pFrame = av_frame_alloc();

    // 创建一个 AVPacket，用来存放下面循环获取到的未解码帧
    packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    av_init_packet(packet);

    int64_t out_channel_layout = AV_CH_LAYOUT_STEREO; //输出声道
    int out_nb_samples = 1024;
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16; // 输出格式 S16
    int out_sample_rate = 44100;
    int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
    int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);

    out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);

    // 初始化SDL系统
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf("Could not initialize SDL - %s\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec spec;
    spec.freq = out_sample_rate;
    spec.format = AUDIO_S16SYS;
    spec.channels = out_channels;
    spec.silence = 0;
    spec.samples = out_nb_samples;
    spec.callback = read_audio_data;
    spec.userdata = pCodecCtx;

    if (SDL_OpenAudio(&spec, NULL) < 0) {
        printf("can't open audio.\n");
        return;
    }

    in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
    au_convert_ctx = swr_alloc();
    au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate, in_channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, NULL);
    swr_init(au_convert_ctx);

    SDL_PauseAudio(0);

    while (av_read_frame(pFormatCtx, packet) >= 0)
    {
        if (packet->stream_index == audioindex)
        {
            avcodec_send_packet(pCodecCtx, packet);
            while (avcodec_receive_frame(pCodecCtx, pFrame) == 0)
            {
                swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t **)pFrame->data, pFrame->nb_samples); // 转换音频
            }

            audio_chunk = (Uint8 *)out_buffer;
            audio_len = out_buffer_size;
            audio_pos = audio_chunk;

            while (audio_len > 0) {
                SDL_Delay(1); // 延迟播放
            }
        }
        av_packet_unref(packet);
    }
    swr_free(&au_convert_ctx);
}
