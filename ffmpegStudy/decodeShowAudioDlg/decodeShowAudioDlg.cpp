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

    AVCodecContext *pCodecCtx;      // 编码相关上下文结构体，保存了文件编解码相关信息
    AVCodec *pCodec;                // 文件具体编码方式对应的那一种编解码器结构体
    AVCodecParserContext *pCodecParserCtx; // 解析编码帧相关上下文结构体，从编码码流中分离出每一帧的编码数据
    AVFrame *pFrame;                // 存储解码后的一帧数据
    AVPacket *packet;               // 存储解码前的一帧数据
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
    // 输入 mp3, aac 等音频压缩文件，进行 解码->pcm->播放 FFmpeg/doc/examples/decode_audio.c
    dlg->m_browse.GetWindowTextW(strInFileName);
    if (strInFileName.IsEmpty()) {
        return 1;
    }
    USES_CONVERSION;
    chInFileName = W2A(strInFileName);

    FILE* fpIn = fopen(chInFileName, "rb");
    FILE* fpOut = fopen(OUTPUT_PCM_FILENAME, "wb");

    // 查找解码器，填充编码方式对应的那一种编解码器结构体 pCodec
    pCodec = avcodec_find_decoder(codec_id);
    if (pCodec == NULL) {
        AfxMessageBox(_T("Could not found decoder."));
        return -1;
    }

    // 填充编码相关上下文结构体 pCodecCtx
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (pCodecCtx == NULL) {
        AfxMessageBox(_T("Could not alloc codec context."));
        return -1;
    }

    // 填充解析编码帧相关上下文结构体
    pCodecParserCtx = av_parser_init(codec_id);
    if (pCodecParserCtx == NULL) {
        AfxMessageBox(_T("Could not init parser."));
        return -1;
    }

    // 打开编解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        AfxMessageBox(_T("Could not open decoder."));
        return -1;
    }

    // 创建 AVFrame 和 AVPacket，用来保存解码前后的数据
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
                // 解码一帧压缩数据
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
                        outBufferSize = frameSize * channels * sampleBytes; // 一帧 pcm 数据缓冲区大小：采样点个数 * 通道个数 * 每个采样值字节数(float)
                        outBuffer = new unsigned char[outBufferSize];

                        //SDL_AudioSpec
                        SDL_AudioSpec wanted_spec;
                        wanted_spec.freq = sampleRate;
                        wanted_spec.format = AUDIO_F32SYS;  // 由 pFrame->format 转化而来
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
                            fwrite(pFrame->data[j] + sampleBytes * i, 1, sampleBytes, fpOut); // 左右通道采样值交替存储
                            memcpy(outBuffer + sampleBytes * k, pFrame->data[j] + sampleBytes * i, sampleBytes); // 拷贝一帧解码数据到缓冲区
                            k++;
                        }
                    }

                    audio_chunk = (Uint8 *)outBuffer;
                    audio_len = outBufferSize;
                    audio_pos = audio_chunk;

                    while (audio_len > 0) // 等待上次解码后的音频播放完毕，然后再解码下一帧
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
