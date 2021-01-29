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

#define OUTPUT_PCM_FILENAME     "temp_output.pcm"

static Uint8 *audio_chunk;      // 一帧音频采样数据缓存区
static Uint32 audio_len;        // 采样数据缓存区剩余未播放的长度
static Uint8 *audio_pos;        // 采样数据缓存区当前播放的位置

// 音频设备需要更多数据的时候会调用该回调函数，应用程序被动推送数据到音频设备
static void fill_audio(void *udata, Uint8 *stream, int len)
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
static UINT threadFuncPlay(LPVOID lpParam)
{
    CffmpegSdlAudioDlg *dlg = (CffmpegSdlAudioDlg *)lpParam;

    AVFormatContext *pFormatCtx;    // 封装相关上下文结构体，也是统领全局的结构体，保存了文件封装格式相关信息
    AVCodecContext *pCodecCtx;      // 编码相关上下文结构体，保存了文件编解码相关信息
    AVCodec *pCodec;                // 文件具体编码方式对应的那一种编解码器结构体
    AVFrame *pFrame;                // 存储解码后的一帧数据
    AVPacket *packet;               // 存储解码前的一帧数据

    unsigned int i, j, k;
    int audioIndex, ret, gotPicture, sampleRate, sampleBytes, channels, frameSize, bufferSize;
    unsigned char *buffer;

    CString strInFileName;
    char * chInFileName;
    // 输入 flv, mp4, mp3, aac 等带音频的文件，进行 解封装->解码->pcm->播放 https://blog.csdn.net/leixiaohua1020/article/details/38979615
    dlg->m_browse.GetWindowTextW(strInFileName);
    if (strInFileName.IsEmpty()) {
        return 1;
    }
    USES_CONVERSION;
    chInFileName = W2A(strInFileName);

    FILE* fpOut = fopen(OUTPUT_PCM_FILENAME, "wb");

    // 网络组件初始化
    avformat_network_init();

    // 打开输入的音视频文件，填充封装相关上下文结构体 pFormatCtx
    pFormatCtx = avformat_alloc_context();
    if (avformat_open_input(&pFormatCtx, chInFileName, NULL, NULL) != 0) {
        AfxMessageBox(_T("Couldn't open input stream."));
        return 1;
    }

    // 读取音视频文件信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        AfxMessageBox(_T("Couldn't find stream information."));
        return 1;
    }

    // 每个音视频文件有多个流(视频流、音频流、字幕流等，而且可有多个)，循环遍历找到音频流
    audioIndex = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioIndex = i;
            break;
        }
    }
    if (audioIndex == -1) {
        AfxMessageBox(_T("Didn't find a audio stream."));
        return 1;
    }

    // 填充编码相关上下文结构体 pCodecCtx
    pCodecCtx = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[audioIndex]->codecpar);

    // 查找解码器，填充编码方式对应的那一种编解码器结构体 pCodec
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        AfxMessageBox(_T("Could not found decoder."));
        return 1;
    }

    // 打开编解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        AfxMessageBox(_T("Could not open decoder."));
        return 1;
    }

    // 创建 AVFrame 和 AVPacket，用来保存解码前后的数据
    pFrame = av_frame_alloc();
    packet = av_packet_alloc();

    sampleBytes = av_get_bytes_per_sample(pCodecCtx->sample_fmt); // AV_SAMPLE_FMT_FLTP
    channels = pCodecCtx->channels;
    frameSize = pCodecCtx->frame_size;
    sampleRate = pCodecCtx->sample_rate;
    bufferSize = frameSize * channels * sampleBytes; // 一帧 pcm 数据缓冲区大小：采样点个数 * 通道个数 * 每个采样值字节数(float)
    buffer = new unsigned char[bufferSize];

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        AfxMessageBox(_T("Couldn't initialize SDL."));
        return 1;
    }

    //SDL_AudioSpec
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = sampleRate;
    wanted_spec.format = AUDIO_F32SYS;  // 由 pCodecCtx->sample_fmt 转化而来
    wanted_spec.channels = channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = frameSize;
    wanted_spec.callback = fill_audio;

    if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
        AfxMessageBox(_T("Couldn't open audio."));
        return 1;
    }

    SDL_PauseAudio(0);

    while (av_read_frame(pFormatCtx, packet) == 0) // 0 for OK, < 0 for error or end of file
    {
        if (packet->stream_index == audioIndex)
        {
            // 解码一帧压缩数据
            ret = avcodec_send_packet(pCodecCtx, packet);
            if (ret < 0) {
                AfxMessageBox(_T("Could not decode."));
                return 1;
            }
            gotPicture = avcodec_receive_frame(pCodecCtx, pFrame);
            if (!gotPicture) {
                k = 0;
                for (i = 0; i < frameSize; i++) {
                    for (j = 0; j < channels; j++) {
                        fwrite(pFrame->data[j] + sampleBytes * i, 1, sampleBytes, fpOut); // 左右通道采样值交替存储
                        memcpy(buffer + sampleBytes * k, pFrame->data[j] + sampleBytes * i, sampleBytes); // 拷贝一帧解码数据到缓冲区
                        k++;
                    }
                }

                audio_chunk = (Uint8 *)buffer;
                audio_len = bufferSize;
                audio_pos = audio_chunk;

                while (audio_len > 0) // 等待上次解码后的音频播放完毕，然后再解码下一帧
                    SDL_Delay(1);
            }
        }
    }

    delete[] buffer;
    fclose(fpOut);
    av_frame_free(&pFrame);
    av_packet_free(&packet);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    SDL_Quit();

    return 0;
}


void CffmpegSdlAudioDlg::OnClickedButtonPlayAudio()
{
    // TODO: Add your control notification handler code here
    AfxBeginThread(threadFuncPlay, this);
}
