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


#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)

int thread_exit = 0;

int sfp_refresh_thread(void *opaque)
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


void CffmpegSdlVideoDlg::OnClickedButtonPlayVideo()
{
    // TODO: Add your control notification handler code here
    AVFormatContext *pFormatCtx;    // 封装格式上下文结构体，也是统领全局的结构体，保存了视频文件封装格式的相关信息
    AVCodecContext *pCodecCtx;      // 编码器上下文结构体，保存了视频音频编解码相关信息
    AVCodec *pCodec;                // 每种视频音频编解码器对应一个该结构体
    AVFrame *pFrame, *pFrameYUV;    // 存储一帧解码后像素或采样数据
    AVPacket *packet;               // 存储一帧压缩编码数据
    unsigned char *out_buffer;      // 存储图像数据
    unsigned int i;
    int videoindex, ret, got_picture;

    //------------SDL----------------
    int screen_w, screen_h;
    SDL_Window *screen;             // 显示窗口
    SDL_Renderer* sdlRenderer;      // 渲染器
    SDL_Texture* sdlTexture;        // 纹理
    SDL_Rect sdlRect;               // 矩形
    SDL_Event event;
    struct SwsContext *img_convert_ctx;

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

    // 每个视频文件中有多个流(视频流、音频流、字幕流等，而且可有多个)，循环遍历找到视频流
    videoindex = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            break;
        }
    }
    if (videoindex == -1) {
        printf("Didn't find a video stream.\n");
        return;
    }

    // 保存视频流中的 AVCodecContext
    pCodecCtx = avcodec_alloc_context3(NULL);
    if (pCodecCtx == NULL)
    {
        printf("Could not allocate AVCodecContext\n");
        return;
    }
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoindex]->codecpar);

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
    pFrameYUV = av_frame_alloc();
    out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));

    // 根据指定的图像参数和提供的数组设置数据指针和线条
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

    // 初始化一个SwsContext
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                     pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

    // 初始化SDL系统
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf("Could not initialize SDL - %s\n", SDL_GetError());
        return;
    }

    // 保存视频的实际宽高
    screen_w = pCodecCtx->width;
    screen_h = pCodecCtx->height;

    // 创建显示窗口
    // screen = SDL_CreateWindow("Simplest ffmpeg player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    //    screen_w, screen_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    screen = SDL_CreateWindowFrom((void *)(GetDlgItem(IDC_STATIC_SHOW)->GetSafeHwnd()));
    if (!screen) {
        printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
        return;
    }

    // 创建渲染器
    sdlRenderer = SDL_CreateRenderer(screen, -1, 0);

    // IYUV: Y + U + V  (3 planes)
    // YV12: Y + V + U  (3 planes)
    // 创建纹理SDL_Texture
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

    sdlRect.x = 0;
    sdlRect.y = 0;
    sdlRect.w = screen_w;
    sdlRect.h = screen_h;

    // 创建一个 AVPacket，用来存放下面循环获取到的未解码帧
    packet = (AVPacket *)av_malloc(sizeof(AVPacket));

    SDL_CreateThread(sfp_refresh_thread, NULL, NULL);

    for (;;)
    {
        SDL_WaitEvent(&event);
        if (event.type == SFM_REFRESH_EVENT)
        {
            while (1)
            {
                if (av_read_frame(pFormatCtx, packet) < 0) // 从输入文件读取一帧压缩数据，如果数据读取完成则停止播放
                    thread_exit = 1;

                if (packet->stream_index == videoindex) // 循环找到视频流
                    break;
            }

            // 解码一帧压缩数据
            ret = avcodec_send_packet(pCodecCtx, packet);
            got_picture = avcodec_receive_frame(pCodecCtx, pFrame);
            if (ret < 0) {
                printf("Decode Error.\n");
                return;
            }
            if (! got_picture) {
                sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
                SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);  // 设置纹理数据
                SDL_RenderClear(sdlRenderer);  // 清理渲染器
                // SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
                SDL_RenderCopy(sdlRenderer, sdlTexture, &sdlRect, &sdlRect); // 将纹理数据copy到渲染器, 将sdlRect区域的图像进行显示
                SDL_RenderPresent(sdlRenderer);     // 显示
            }
            av_packet_unref(packet);
        }
        else if (event.type == SDL_QUIT)
        {
            thread_exit = 1;
        }
        else if (event.type == SFM_BREAK_EVENT)
        {
            break;
        }
    }

    GetDlgItem(IDC_STATIC_SHOW)->ShowWindow(SW_SHOWNORMAL);
    sws_freeContext(img_convert_ctx);
    SDL_Quit();
    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);   // 关闭解码器
    avformat_close_input(&pFormatCtx); // 关闭输入视频文件
}
