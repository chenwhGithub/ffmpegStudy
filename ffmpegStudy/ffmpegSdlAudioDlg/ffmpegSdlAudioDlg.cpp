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

static Uint8 *audio_chunk;      // һ֡��Ƶ�������ݻ�����
static Uint32 audio_len;        // �������ݻ�����ʣ��δ���ŵĳ���
static Uint8 *audio_pos;        // �������ݻ�������ǰ���ŵ�λ��

// ��Ƶ�豸��Ҫ�������ݵ�ʱ�����øûص�������Ӧ�ó��򱻶��������ݵ���Ƶ�豸
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

    AVFormatContext *pFormatCtx;    // ��װ��������Ľṹ�壬Ҳ��ͳ��ȫ�ֵĽṹ�壬�������ļ���װ��ʽ�����Ϣ
    AVCodecContext *pCodecCtx;      // ������������Ľṹ�壬�������ļ�����������Ϣ
    AVCodec *pCodec;                // �ļ�������뷽ʽ��Ӧ����һ�ֱ�������ṹ��
    AVFrame *pFrame;                // �洢������һ֡����
    AVPacket *packet;               // �洢����ǰ��һ֡����

    unsigned int i, j, k;
    int audioIndex, ret, gotPicture, sampleRate, sampleBytes, channels, frameSize, bufferSize;
    unsigned char *buffer;

    CString strInFileName;
    char * chInFileName;
    // ���� flv, mp4, mp3, aac �ȴ���Ƶ���ļ������� ���װ->����->pcm->���� https://blog.csdn.net/leixiaohua1020/article/details/38979615
    dlg->m_browse.GetWindowTextW(strInFileName);
    if (strInFileName.IsEmpty()) {
        return 1;
    }
    USES_CONVERSION;
    chInFileName = W2A(strInFileName);

    FILE* fpOut = fopen(OUTPUT_PCM_FILENAME, "wb");

    // ���������ʼ��
    avformat_network_init();

    // �����������Ƶ�ļ�������װ��������Ľṹ�� pFormatCtx
    pFormatCtx = avformat_alloc_context();
    if (avformat_open_input(&pFormatCtx, chInFileName, NULL, NULL) != 0) {
        AfxMessageBox(_T("Couldn't open input stream."));
        return 1;
    }

    // ��ȡ����Ƶ�ļ���Ϣ
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        AfxMessageBox(_T("Couldn't find stream information."));
        return 1;
    }

    // ÿ������Ƶ�ļ��ж����(��Ƶ������Ƶ������Ļ���ȣ����ҿ��ж��)��ѭ�������ҵ���Ƶ��
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

    // ��������������Ľṹ�� pCodecCtx
    pCodecCtx = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[audioIndex]->codecpar);

    // ���ҽ������������뷽ʽ��Ӧ����һ�ֱ�������ṹ�� pCodec
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        AfxMessageBox(_T("Could not found decoder."));
        return 1;
    }

    // �򿪱������
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        AfxMessageBox(_T("Could not open decoder."));
        return 1;
    }

    // ���� AVFrame �� AVPacket�������������ǰ�������
    pFrame = av_frame_alloc();
    packet = av_packet_alloc();

    sampleBytes = av_get_bytes_per_sample(pCodecCtx->sample_fmt); // AV_SAMPLE_FMT_FLTP
    channels = pCodecCtx->channels;
    frameSize = pCodecCtx->frame_size;
    sampleRate = pCodecCtx->sample_rate;
    bufferSize = frameSize * channels * sampleBytes; // һ֡ pcm ���ݻ�������С����������� * ͨ������ * ÿ������ֵ�ֽ���(float)
    buffer = new unsigned char[bufferSize];

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        AfxMessageBox(_T("Couldn't initialize SDL."));
        return 1;
    }

    //SDL_AudioSpec
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = sampleRate;
    wanted_spec.format = AUDIO_F32SYS;  // �� pCodecCtx->sample_fmt ת������
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
            // ����һ֡ѹ������
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
                        fwrite(pFrame->data[j] + sampleBytes * i, 1, sampleBytes, fpOut); // ����ͨ������ֵ����洢
                        memcpy(buffer + sampleBytes * k, pFrame->data[j] + sampleBytes * i, sampleBytes); // ����һ֡�������ݵ�������
                        k++;
                    }
                }

                audio_chunk = (Uint8 *)buffer;
                audio_len = bufferSize;
                audio_pos = audio_chunk;

                while (audio_len > 0) // �ȴ��ϴν�������Ƶ������ϣ�Ȼ���ٽ�����һ֡
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
