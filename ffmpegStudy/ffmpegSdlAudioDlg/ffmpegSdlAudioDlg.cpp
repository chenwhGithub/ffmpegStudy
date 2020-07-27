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

// ��Ƶ�豸��Ҫ�������ݵ�ʱ�����øûص�����
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
    AVFormatContext *pFormatCtx;    // ��װ��ʽ�����Ľṹ�壬Ҳ��ͳ��ȫ�ֵĽṹ�壬��������Ƶ�ļ���װ��ʽ�������Ϣ
    AVCodecContext *pCodecCtx;      // �����������Ľṹ�壬��������Ƶ��Ƶ����������Ϣ
    AVCodec *pCodec;                // ÿ����Ƶ��Ƶ���������Ӧһ���ýṹ��
    AVFrame *pFrame;                // �洢һ֡��������ػ��������
    AVPacket *packet;               // �洢һ֡ѹ����������
    unsigned char *out_buffer;      // �洢ͼ������
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
    avformat_network_init();        // ���������ȫ�ֳ�ʼ��
    pFormatCtx = avformat_alloc_context();  // ���� AVFormatContext

    // �����������Ƶ�ļ�
    if (avformat_open_input(&pFormatCtx, chFileName, NULL, NULL) != 0) {
        printf("Couldn't open input stream.\n");
        return;
    }

    // ��ȡ����Ƶ�ļ���Ϣ
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        printf("Couldn't find stream information.\n");
        return;
    }
    /*
    // ÿ����Ƶ�ļ����ж����(��Ƶ������Ƶ������Ļ���ȣ����ҿ��ж��)��ѭ�������ҵ���Ƶ��
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

    // ������Ƶ���е� AVCodecContext
    pCodecCtx = avcodec_alloc_context3(NULL);
    if (pCodecCtx == NULL)
    {
        printf("Could not allocate AVCodecContext\n");
        return;
    }
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[audioindex]->codecpar);

    // ���ҽ�����
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        printf("Codec not found.\n");
        return;
    }

    // �򿪽�����
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec.\n");
        return;
    }

    // ���� AVFrame��������Ž�����һ֡������
    pFrame = av_frame_alloc();

    // ����һ�� AVPacket�������������ѭ����ȡ����δ����֡
    packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    av_init_packet(packet);

    int64_t out_channel_layout = AV_CH_LAYOUT_STEREO; //�������
    int out_nb_samples = 1024;
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16; // �����ʽ S16
    int out_sample_rate = 44100;
    int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
    int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);

    out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);

    // ��ʼ��SDLϵͳ
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
                swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t **)pFrame->data, pFrame->nb_samples); // ת����Ƶ
            }

            audio_chunk = (Uint8 *)out_buffer;
            audio_len = out_buffer_size;
            audio_pos = audio_chunk;

            while (audio_len > 0) {
                SDL_Delay(1); // �ӳٲ���
            }
        }
        av_packet_unref(packet);
    }
    swr_free(&au_convert_ctx);
}
