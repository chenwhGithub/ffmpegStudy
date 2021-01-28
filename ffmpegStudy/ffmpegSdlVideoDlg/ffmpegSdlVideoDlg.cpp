// C:\code\H264\ffmpegStudy\ffmpegStudy\ffmpegSdlVideoDlg\ffmpegSdlVideoDlg.cpp : implementation file
//

#include "../pch.h"
#include "../ffmpegStudy.h"
#include "ffmpegSdlVideoDlg.h"
#include "afxdialogex.h"

#ifdef __cplusplus
extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/codec.h"
#include "libavcodec/avcodec.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}
#endif

#include "SDL.h"


#define SFM_REFRESH_EVENT   (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT     (SDL_USEREVENT + 2)

#define OUTPUT_YUV420P      0 // 解码后是否保存 YUV 信息到文件

static int thread_exit = 0;


// CffmpegSdlVideoDlg dialog

IMPLEMENT_DYNAMIC(CffmpegSdlVideoDlg, CDialogEx)

CffmpegSdlVideoDlg::CffmpegSdlVideoDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_FFMPEGSDL_VIDEO, pParent)
{

}

CffmpegSdlVideoDlg::~CffmpegSdlVideoDlg()
{
}

void CffmpegSdlVideoDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MFCEDITBROWSE, m_browse);
}


BEGIN_MESSAGE_MAP(CffmpegSdlVideoDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_PLAY_VIDEO, &CffmpegSdlVideoDlg::OnClickedButtonPlayVideo)
END_MESSAGE_MAP()


// CffmpegSdlVideoDlg message handlers
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
    CffmpegSdlVideoDlg *dlg = (CffmpegSdlVideoDlg *)lpParam;

    AVFormatContext *pFormatCtx;    // 封装相关上下文结构体，也是统领全局的结构体，保存了文件封装格式相关信息
    AVCodecContext *pCodecCtx;      // 编码相关上下文结构体，保存了文件编解码相关信息
    AVCodec *pCodec;                // 文件具体编码方式对应的那一种编解码器结构体
    AVFrame *pFrame, *pFrameYUV;    // 存储解码后的一帧数据
    AVPacket *packet;               // 存储解码前的一帧数据
    unsigned char *outBuffer;
    unsigned int i;
    int videoIndex, ret, gotPicture;

    //------------SDL----------------
    SDL_Window *screen;             // 显示窗口
    SDL_Renderer* sdlRenderer;      // 渲染器
    SDL_Texture* sdlTexture;        // 纹理
    SDL_Rect sdlRect;
    SDL_Event event;
    struct SwsContext* imgConvertCtx;
    int frameWidth, frameHeight, frameSize;

    CString strFileName;
    char * chFileName;
    // 输入 flv, mp4, mov 等带封装的视频文件，进行 解封装->解码->显示 https://blog.csdn.net/leixiaohua1020/article/details/38868499
    dlg->m_browse.GetWindowTextW(strFileName);
    if (strFileName.IsEmpty()) {
        return 1;
    }
    USES_CONVERSION;
    chFileName = W2A(strFileName);

#if OUTPUT_YUV420P
    FILE* fp_yuv = fopen("Forrest_Gump_IMAX_640_352.yuv", "wb+");
#endif

    // av_register_all();
    avformat_network_init(); // 网络组件全局初始化

    // 打开输入的音视频文件，填充封装相关上下文结构体 pFormatCtx
    pFormatCtx = avformat_alloc_context();
    if (avformat_open_input(&pFormatCtx, chFileName, NULL, NULL) != 0) {
        AfxMessageBox(_T("Couldn't open input stream."));
        return 1;
    }

    // 读取音视频文件信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        AfxMessageBox(_T("Couldn't find stream information."));
        return 1;
    }

    // 每个音视频文件有多个流(视频流、音频流、字幕流等，而且可有多个)，循环遍历找到视频流
    videoIndex = -1;
    for (i=0; i<pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
            break;
        }
    }
    if (videoIndex == -1) {
        AfxMessageBox(_T("Didn't find a video stream."));
        return 1;
    }

    // 填充编码相关上下文结构体 pCodecCtx
    pCodecCtx = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoIndex]->codecpar);

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
    pFrameYUV = av_frame_alloc();
    packet = (AVPacket *)av_malloc(sizeof(AVPacket));

    // 分配存储一帧图像 YUV 信息所需 buffer
    outBuffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
    // 根据指定的图像参数和提供的数组设置数据指针和线条，解码后的一帧数据填充到 buffer
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, outBuffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

    // 初始化 SwsContext
    imgConvertCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                   pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

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
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

    // 保存视频帧的宽和高
    frameWidth = pCodecCtx->width;
    frameHeight = pCodecCtx->height;
    // YUV420P, Y:w*h  U:w*h/4  V:w*h/4
    frameSize = frameWidth * frameHeight;

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
            while (1) // 循环读取找到一帧压缩的视频数据
            {
                if (av_read_frame(pFormatCtx, packet) < 0) { // 0 for OK, < 0 for error or end of file
                    thread_exit = 1;
                    break;
                }

                if (packet->stream_index == videoIndex)
                    break;
            }

            // 解码一帧压缩数据
            ret = avcodec_send_packet(pCodecCtx, packet);
            gotPicture = avcodec_receive_frame(pCodecCtx, pFrame);
            if (ret < 0) {
                AfxMessageBox(_T("Could not decode."));
                return 1;
            }
            if (!gotPicture) {
                // 解码后的 pFrame 包含无效数据，以第1行亮度数据 pFrame->data[0] 为例，0-479存储的是有效数据，而480-511存储的是无效数据，因此需要使用 sw_scale() 进行转换去除无效数据
                sws_scale(imgConvertCtx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);

#if OUTPUT_YUV420P
                fwrite(pFrameYUV->data[0], 1, frameSize, fp_yuv);     // Y
                fwrite(pFrameYUV->data[1], 1, frameSize/4, fp_yuv);   // U
                fwrite(pFrameYUV->data[2], 1, frameSize/4, fp_yuv);   // V
#endif

                SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]); // 设置纹理数据
                SDL_RenderClear(sdlRenderer); // 清理渲染器
                // SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
                SDL_RenderCopy(sdlRenderer, sdlTexture, &sdlRect, &sdlRect); // 将纹理数据 copy 到渲染器
                SDL_RenderPresent(sdlRenderer); // 渲染器显示
            }
            av_packet_unref(packet);
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

#if OUTPUT_YUV420P
    fclose(fp_yuv);
#endif

    sws_freeContext(imgConvertCtx);
    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    SDL_Quit();
    dlg->GetDlgItem(IDC_STATIC_SHOW)->ShowWindow(SW_SHOWNORMAL); // SDL Hide Window When it finished, so involved after SDL_Quit()

    return 0;
}


void CffmpegSdlVideoDlg::OnClickedButtonPlayVideo()
{
    AfxBeginThread(threadFuncPlay, this);
}
