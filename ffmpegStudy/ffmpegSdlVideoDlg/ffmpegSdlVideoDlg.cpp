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

#define OUTPUT_YUV420P      0 // ������Ƿ񱣴� YUV ��Ϣ���ļ�

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

    AVFormatContext *pFormatCtx;    // ��װ��������Ľṹ�壬Ҳ��ͳ��ȫ�ֵĽṹ�壬�������ļ���װ��ʽ�����Ϣ
    AVCodecContext *pCodecCtx;      // ������������Ľṹ�壬�������ļ�����������Ϣ
    AVCodec *pCodec;                // �ļ�������뷽ʽ��Ӧ����һ�ֱ�������ṹ��
    AVFrame *pFrame, *pFrameYUV;    // �洢������һ֡����
    AVPacket *packet;               // �洢����ǰ��һ֡����
    unsigned char *outBuffer;
    unsigned int i;
    int videoIndex, ret, gotPicture;

    //------------SDL----------------
    SDL_Window *screen;             // ��ʾ����
    SDL_Renderer* sdlRenderer;      // ��Ⱦ��
    SDL_Texture* sdlTexture;        // ����
    SDL_Rect sdlRect;
    SDL_Event event;
    struct SwsContext* imgConvertCtx;
    int frameWidth, frameHeight, frameSize;

    CString strFileName;
    char * chFileName;
    // ���� flv, mp4, mov �ȴ���װ����Ƶ�ļ������� ���װ->����->��ʾ https://blog.csdn.net/leixiaohua1020/article/details/38868499
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
    avformat_network_init(); // �������ȫ�ֳ�ʼ��

    // �����������Ƶ�ļ�������װ��������Ľṹ�� pFormatCtx
    pFormatCtx = avformat_alloc_context();
    if (avformat_open_input(&pFormatCtx, chFileName, NULL, NULL) != 0) {
        AfxMessageBox(_T("Couldn't open input stream."));
        return 1;
    }

    // ��ȡ����Ƶ�ļ���Ϣ
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        AfxMessageBox(_T("Couldn't find stream information."));
        return 1;
    }

    // ÿ������Ƶ�ļ��ж����(��Ƶ������Ƶ������Ļ���ȣ����ҿ��ж��)��ѭ�������ҵ���Ƶ��
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

    // ��������������Ľṹ�� pCodecCtx
    pCodecCtx = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoIndex]->codecpar);

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
    pFrameYUV = av_frame_alloc();
    packet = (AVPacket *)av_malloc(sizeof(AVPacket));

    // ����洢һ֡ͼ�� YUV ��Ϣ���� buffer
    outBuffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
    // ����ָ����ͼ��������ṩ��������������ָ���������������һ֡������䵽 buffer
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, outBuffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

    // ��ʼ�� SwsContext
    imgConvertCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                   pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

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
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

    // ������Ƶ֡�Ŀ�͸�
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
            while (1) // ѭ����ȡ�ҵ�һ֡ѹ������Ƶ����
            {
                if (av_read_frame(pFormatCtx, packet) < 0) { // 0 for OK, < 0 for error or end of file
                    thread_exit = 1;
                    break;
                }

                if (packet->stream_index == videoIndex)
                    break;
            }

            // ����һ֡ѹ������
            ret = avcodec_send_packet(pCodecCtx, packet);
            gotPicture = avcodec_receive_frame(pCodecCtx, pFrame);
            if (ret < 0) {
                AfxMessageBox(_T("Could not decode."));
                return 1;
            }
            if (!gotPicture) {
                // ������ pFrame ������Ч���ݣ��Ե�1���������� pFrame->data[0] Ϊ����0-479�洢������Ч���ݣ���480-511�洢������Ч���ݣ������Ҫʹ�� sw_scale() ����ת��ȥ����Ч����
                sws_scale(imgConvertCtx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);

#if OUTPUT_YUV420P
                fwrite(pFrameYUV->data[0], 1, frameSize, fp_yuv);     // Y
                fwrite(pFrameYUV->data[1], 1, frameSize/4, fp_yuv);   // U
                fwrite(pFrameYUV->data[2], 1, frameSize/4, fp_yuv);   // V
#endif

                SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]); // ������������
                SDL_RenderClear(sdlRenderer); // ������Ⱦ��
                // SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
                SDL_RenderCopy(sdlRenderer, sdlTexture, &sdlRect, &sdlRect); // ���������� copy ����Ⱦ��
                SDL_RenderPresent(sdlRenderer); // ��Ⱦ����ʾ
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
