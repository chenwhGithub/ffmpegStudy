// C:\code\FFmpeg\ffmpegStudy\ffmpegStudy\decodeShowAudioDlg\decodeShowAudioDlg.cpp : implementation file
//

#include "../pch.h"
#include "../ffmpegStudy.h"
#include "decodeShowAudioDlg.h"
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

// CDecodeShowAudioDlg dialog

IMPLEMENT_DYNAMIC(CDecodeShowAudioDlg, CDialogEx)

CDecodeShowAudioDlg::CDecodeShowAudioDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_DECODESHOWAUDIO, pParent)
{

}

CDecodeShowAudioDlg::~CDecodeShowAudioDlg()
{
}

void CDecodeShowAudioDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MFCEDITBROWSE, m_browse);
}


BEGIN_MESSAGE_MAP(CDecodeShowAudioDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_PLAY, &CDecodeShowAudioDlg::OnClickedButtonPlay)
END_MESSAGE_MAP()


// CDecodeShowAudioDlg message handlers
static UINT threadFuncPlay(LPVOID lpParam)
{
    CDecodeShowAudioDlg *dlg = (CDecodeShowAudioDlg *)lpParam;

    AVCodecContext *pCodecCtx;      // ������������Ľṹ�壬�������ļ�����������Ϣ
    AVCodec *pCodec;                // �ļ�������뷽ʽ��Ӧ����һ�ֱ�������ṹ��
    AVCodecParserContext *pCodecParserCtx; // ��������֡��������Ľṹ�壬�ӱ��������з����ÿһ֡�ı�������
    AVFrame *pFrame;                // �洢������һ֡����
    AVPacket *packet;               // �洢����ǰ��һ֡����
    AVCodecID codec_id = AV_CODEC_ID_MP3;

    const int inBufferSize = 20480;
    unsigned char inBuffer[inBufferSize + AV_INPUT_BUFFER_PADDING_SIZE] = { 0 };
    unsigned char *curPtr;
    unsigned int outBufferSize;
    unsigned char *outBuffer = NULL;
    size_t curSize;
    int ret, len, gotPicture, sampleRate, sampleBytes, channels, frameSize, firstFrameParsed = 0;
    unsigned int i, j, k;

    CString strInFileName;
    char * chInFileName;
    // ���� mp3, aac ����Ƶѹ���ļ������� ����->pcm->���� FFmpeg/doc/examples/decode_audio.c
    dlg->m_browse.GetWindowTextW(strInFileName);
    if (strInFileName.IsEmpty()) {
        return 1;
    }
    USES_CONVERSION;
    chInFileName = W2A(strInFileName);

    FILE* fpIn = fopen(chInFileName, "rb");
    FILE* fpOut = fopen(OUTPUT_PCM_FILENAME, "wb");

    // ���ҽ������������뷽ʽ��Ӧ����һ�ֱ�������ṹ�� pCodec
    pCodec = avcodec_find_decoder(codec_id);
    if (pCodec == NULL) {
        AfxMessageBox(_T("Could not found decoder."));
        return -1;
    }

    // ��������������Ľṹ�� pCodecCtx
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (pCodecCtx == NULL) {
        AfxMessageBox(_T("Could not alloc codec context."));
        return -1;
    }

    // ����������֡��������Ľṹ��
    pCodecParserCtx = av_parser_init(codec_id);
    if (pCodecParserCtx == NULL) {
        AfxMessageBox(_T("Could not init parser."));
        return -1;
    }

    // �򿪱������
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        AfxMessageBox(_T("Could not open decoder."));
        return -1;
    }

    // ���� AVFrame �� AVPacket�������������ǰ�������
    pFrame = av_frame_alloc();
    packet = av_packet_alloc();

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        AfxMessageBox(_T("Couldn't initialize SDL."));
        return 1;
    }

    while (!feof(fpIn)) {
        curSize = fread(inBuffer, 1, inBufferSize, fpIn);
        if (curSize == 0)
            break;

        curPtr = inBuffer;
        while (curSize > 0) {
            len = av_parser_parse2(pCodecParserCtx, pCodecCtx, &packet->data, &packet->size, curPtr, curSize, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (len < 0) {
                AfxMessageBox(_T("Could not av_parser_parse2."));
                return -1;
            }
            curPtr += len;
            curSize -= len;

            if (packet->size > 0) {
                // ����һ֡ѹ������
                ret = avcodec_send_packet(pCodecCtx, packet);  // error ?
                if (ret < 0) {
                    AfxMessageBox(_T("Could not decode."));
                    return -1;
                }
                gotPicture = avcodec_receive_frame(pCodecCtx, pFrame);
                if (!gotPicture) {
                    if (!firstFrameParsed) {
                        sampleBytes = av_get_bytes_per_sample((AVSampleFormat)pFrame->format); // AV_SAMPLE_FMT_FLTP
                        channels = pFrame->channels;
                        frameSize = pFrame->nb_samples;
                        sampleRate = pFrame->sample_rate;
                        outBufferSize = frameSize * channels * sampleBytes; // һ֡ pcm ���ݻ�������С����������� * ͨ������ * ÿ������ֵ�ֽ���(float)
                        outBuffer = new unsigned char[outBufferSize];

                        //SDL_AudioSpec
                        SDL_AudioSpec wanted_spec;
                        wanted_spec.freq = sampleRate;
                        wanted_spec.format = AUDIO_F32SYS;  // �� pFrame->format ת������
                        wanted_spec.channels = channels;
                        wanted_spec.silence = 0;
                        wanted_spec.samples = frameSize;
                        wanted_spec.callback = fill_audio;

                        if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
                            AfxMessageBox(_T("Couldn't open audio."));
                            return 1;
                        }

                        SDL_PauseAudio(0);

                        firstFrameParsed = 1;
                    }

                    k = 0;
                    for (i = 0; i < frameSize; i++) {
                        for (j = 0; j < channels; j++) {
                            fwrite(pFrame->data[j] + sampleBytes * i, 1, sampleBytes, fpOut); // ����ͨ������ֵ����洢
                            memcpy(outBuffer + sampleBytes * k, pFrame->data[j] + sampleBytes * i, sampleBytes); // ����һ֡�������ݵ�������
                            k++;
                        }
                    }

                    audio_chunk = (Uint8 *)outBuffer;
                    audio_len = outBufferSize;
                    audio_pos = audio_chunk;

                    while (audio_len > 0) // �ȴ��ϴν�������Ƶ������ϣ�Ȼ���ٽ�����һ֡
                        SDL_Delay(1);
                }
            }
        }
    }

    delete[] outBuffer;
    fclose(fpIn);
    fclose(fpOut);
    av_frame_free(&pFrame);
    av_packet_free(&packet);
    avcodec_close(pCodecCtx);
    av_parser_close(pCodecParserCtx);
    SDL_Quit();

    return 0;
}


void CDecodeShowAudioDlg::OnClickedButtonPlay()
{
    // TODO: Add your control notification handler code here
    AfxBeginThread(threadFuncPlay, this);
}
