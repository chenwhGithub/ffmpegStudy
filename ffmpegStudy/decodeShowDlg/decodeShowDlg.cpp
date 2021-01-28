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
    AVCodecContext *pCodecCtx;      // 编码相关上下文结构体，保存了文件编解码相关信息
    AVCodec *pCodec;                // 文件具体编码方式对应的那一种编解码器结构体
    AVCodecParserContext *pCodecParserCtx; // 解析编码帧相关上下文结构体，从编码码流中分离出每一帧的编码数据
    AVFrame *pFrame;                // 存储解码后的一帧数据
    AVPacket *packet;               // 存储解码前的一帧数据
    AVCodecID codec_id = AV_CODEC_ID_H264; // AV_CODEC_ID_HEVC for h265

    const int inBufferSize = 4096;
    unsigned char inBuffer[inBufferSize + AV_INPUT_BUFFER_PADDING_SIZE] = { 0 };
    unsigned char *curPtr;
    size_t curSize;
    int ret, len, frameSize, gotPicture, firstFrameParsed = 0;

    FILE* fpIn = fopen(inFileName, "rb");
    FILE* fpOut = fopen(outFileName, "wb");

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
                ret = avcodec_send_packet(pCodecCtx, packet);
                if (ret < 0) {
                    AfxMessageBox(_T("Could not decode."));
                    return -1;
                }
                gotPicture = avcodec_receive_frame(pCodecCtx, pFrame);
                if (!gotPicture) {
                    if (!firstFrameParsed) {
                        // 保存视频帧的宽和高
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
    SDL_Window *screen;             // 显示窗口
    SDL_Renderer* sdlRenderer;      // 渲染器
    SDL_Texture* sdlTexture;        // 纹理
    SDL_Rect sdlRect;
    SDL_Event event;

    FILE *fp = fopen(OUTPUT_YUV_FILENAME, "rb");
    if (fp == NULL) {
        AfxMessageBox(_T("Could not open yuv file."));
        return 1;
    }

    // 由于没有编解码信息，不知道视频帧尺寸，因此直接播放 yuv 时需要手动设置帧尺寸
    // YUV420P, Y:w*h  U:w*h/4  V:w*h/4
    int frameBytes = frameWidth * frameHeight * 12 / 8;
    unsigned char* buffer = new unsigned char[frameBytes];

    // 初始化 SDL 系统
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        AfxMessageBox(_T("Could not initialize SDL."));
        return 1;
    }

    // 创建显示窗口，可以是弹出窗口，也可以是 MFC 控件(必须在新线程中)
    // screen = SDL_CreateWindow("Simplest ffmpeg player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    //                           frameWidth, frameHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    screen = SDL_CreateWindowFrom((void *)(dlg->GetDlgItem(IDC_STATIC_SHOW)->GetSafeHwnd()));

    // 创建渲染器
    sdlRenderer = SDL_CreateRenderer(screen, -1, 0);

    // IYUV: Y + U + V  (3 planes)
    // YV12: Y + V + U  (3 planes)
    // 创建纹理
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
    // 输入 .h264/.hevc 等编码码流文件，进行 解码->yuv->显示 https://blog.csdn.net/leixiaohua1020/article/details/42181571
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
