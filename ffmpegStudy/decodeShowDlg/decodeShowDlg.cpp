// decodeShowDlg.cpp : implementation file
//

#include "../pch.h"
#include "../ffmpegStudy.h"
#include "decodeShowDlg.h"
#include "afxdialogex.h"

#ifdef __cplusplus
extern "C" {
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}
#endif

#include "SDL.h"

#define SFM_REFRESH_EVENT       (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT         (SDL_USEREVENT + 2)
#define OUTPUT_YUV_FILENAME     "temp_output.yuv"

static int frameWidth = 0, frameHeight = 0;
static int thread_exit = 0;

// CDecodeShowDlg dialog

IMPLEMENT_DYNAMIC(CDecodeShowDlg, CDialogEx)

CDecodeShowDlg::CDecodeShowDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_DECODESHOW, pParent)
{

}

CDecodeShowDlg::~CDecodeShowDlg()
{
}

void CDecodeShowDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MFCEDITBROWSE, m_browse);
}


BEGIN_MESSAGE_MAP(CDecodeShowDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_PLAY, &CDecodeShowDlg::OnClickedButtonPlay)
END_MESSAGE_MAP()


// CDecodeShowDlg message handlers
int CDecodeShowDlg::decodeH264ToYuv(const char* inFileName, const char* outFileName)
{
    AVCodecContext *pCodecCtx;      // ������������Ľṹ�壬�������ļ�����������Ϣ
    AVCodec *pCodec;                // �ļ�������뷽ʽ��Ӧ����һ�ֱ�������ṹ��
    AVCodecParserContext *pCodecParserCtx; // ��������֡��������Ľṹ�壬�ӱ��������з����ÿһ֡�ı�������
    AVFrame *pFrame;                // �洢������һ֡����
    AVPacket *packet;               // �洢����ǰ��һ֡����
    AVCodecID codec_id = AV_CODEC_ID_H264; // AV_CODEC_ID_HEVC for h265

    const int inBufferSize = 4096;
    unsigned char inBuffer[inBufferSize + AV_INPUT_BUFFER_PADDING_SIZE] = { 0 };
    unsigned char *curPtr;
    size_t curSize;
    int ret, len, frameSize, gotPicture, firstFrameParsed = 0;

    FILE* fpIn = fopen(inFileName, "rb");
    FILE* fpOut = fopen(outFileName, "wb");

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
                ret = avcodec_send_packet(pCodecCtx, packet);
                if (ret < 0) {
                    AfxMessageBox(_T("Could not decode."));
                    return -1;
                }
                gotPicture = avcodec_receive_frame(pCodecCtx, pFrame);
                if (!gotPicture) {
                    if (!firstFrameParsed) {
                        // ������Ƶ֡�Ŀ�͸�
                        frameWidth = pFrame->width;
                        frameHeight = pFrame->height;
                        // YUV420P, Y:w*h  U:w*h/4  V:w*h/4
                        frameSize = frameWidth * frameHeight;

                        firstFrameParsed = 1;
                    }

                    fwrite(pFrame->data[0], 1, frameSize, fpOut);     // Y
                    fwrite(pFrame->data[1], 1, frameSize/4, fpOut);   // U
                    fwrite(pFrame->data[2], 1, frameSize/4, fpOut);   // V
                }
            }
        }
    }

    fclose(fpIn);
    fclose(fpOut);
    av_frame_free(&pFrame);
    av_packet_free(&packet);
    av_parser_close(pCodecParserCtx);
    avcodec_close(pCodecCtx);

    return 0;
}


static int threadFuncFresh(void *opaque)
{
    thread_exit = 0;
    SDL_Event event;
    event.type = SFM_REFRESH_EVENT;

    while (!thread_exit)
    {
        SDL_PushEvent(&event);
        SDL_Delay(40);
    }

    event.type = SFM_BREAK_EVENT;
    SDL_PushEvent(&event);

    return 0;
}


static UINT threadFuncPlay(LPVOID lpParam)
{
    CDecodeShowDlg *dlg = (CDecodeShowDlg*)lpParam;

    //------------SDL----------------
    SDL_Window *screen;             // ��ʾ����
    SDL_Renderer* sdlRenderer;      // ��Ⱦ��
    SDL_Texture* sdlTexture;        // ����
    SDL_Rect sdlRect;
    SDL_Event event;

    FILE *fp = fopen(OUTPUT_YUV_FILENAME, "rb");
    if (fp == NULL) {
        AfxMessageBox(_T("Could not open yuv file."));
        return 1;
    }

    // ����û�б������Ϣ����֪����Ƶ֡�ߴ磬���ֱ�Ӳ��� yuv ʱ��Ҫ�ֶ�����֡�ߴ�
    // YUV420P, Y:w*h  U:w*h/4  V:w*h/4
    int frameBytes = frameWidth * frameHeight * 12 / 8;
    unsigned char* buffer = new unsigned char[frameBytes];

    // ��ʼ�� SDL ϵͳ
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        AfxMessageBox(_T("Could not initialize SDL."));
        return 1;
    }

    // ������ʾ���ڣ������ǵ������ڣ�Ҳ������ MFC �ؼ�(���������߳���)
    // screen = SDL_CreateWindow("Simplest ffmpeg player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    //                           frameWidth, frameHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    screen = SDL_CreateWindowFrom((void *)(dlg->GetDlgItem(IDC_STATIC_SHOW)->GetSafeHwnd()));

    // ������Ⱦ��
    sdlRenderer = SDL_CreateRenderer(screen, -1, 0);

    // IYUV: Y + U + V  (3 planes)
    // YV12: Y + V + U  (3 planes)
    // ��������
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, frameWidth, frameHeight);

    sdlRect.x = 0;
    sdlRect.y = 0;
    sdlRect.w = frameWidth;
    sdlRect.h = frameHeight;

    SDL_CreateThread(threadFuncFresh, NULL, NULL);

    while (1)
    {
        SDL_WaitEvent(&event);
        if (event.type == SFM_REFRESH_EVENT)
        {
            if (fread(buffer, 1, frameBytes, fp) != frameBytes) {
                thread_exit = 1;
                break;
            }

            SDL_UpdateTexture(sdlTexture, NULL, buffer, frameWidth);
            SDL_RenderClear(sdlRenderer);
            SDL_RenderCopy(sdlRenderer, sdlTexture, &sdlRect, &sdlRect);
            SDL_RenderPresent(sdlRenderer);
        }
        else if (event.type == SFM_BREAK_EVENT)
        {
            break;
        }
        else if (event.type == SDL_QUIT)
        {
            thread_exit = 1;
        }
    }

    delete[] buffer;
    fclose(fp);
    SDL_Quit();
    dlg->GetDlgItem(IDC_STATIC_SHOW)->ShowWindow(SW_SHOWNORMAL); // SDL Hide Window When it finished, so involved after SDL_Quit()

    return 0;
}


void CDecodeShowDlg::OnClickedButtonPlay()
{
    // TODO: Add your control notification handler code here
    CString strInFileName;
    char * chInFileName;
    // ���� .h264/.hevc �ȱ��������ļ������� ����->yuv->��ʾ https://blog.csdn.net/leixiaohua1020/article/details/42181571
    m_browse.GetWindowTextW(strInFileName);
    if (strInFileName.IsEmpty()) {
        return;
    }
    USES_CONVERSION;
    chInFileName = W2A(strInFileName);

    int ret = decodeH264ToYuv(chInFileName, OUTPUT_YUV_FILENAME);
    if (!ret) {
        AfxBeginThread(threadFuncPlay, this);
    }
}
