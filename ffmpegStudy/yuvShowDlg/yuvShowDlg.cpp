// YuvShowDlg.cpp : implementation file
//

#include "../pch.h"
#include "../ffmpegStudy.h"
#include "YuvShowDlg.h"
#include "afxdialogex.h"

#include "SDL.h"

#define SFM_REFRESH_EVENT   (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT     (SDL_USEREVENT + 2)

static int thread_exit = 0;


// CYuvShowDlg dialog

IMPLEMENT_DYNAMIC(CYuvShowDlg, CDialogEx)

CYuvShowDlg::CYuvShowDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_YUVSHOW, pParent)
{

}

CYuvShowDlg::~CYuvShowDlg()
{
}

void CYuvShowDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MFCEDITBROWSE, m_browse);
}


BEGIN_MESSAGE_MAP(CYuvShowDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_PLAY, &CYuvShowDlg::OnClickedButtonPlay)
END_MESSAGE_MAP()


// CYuvShowDlg message handlers
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
    CYuvShowDlg *dlg = (CYuvShowDlg *)lpParam;

    //------------SDL----------------
    SDL_Window *screen;             // ��ʾ����
    SDL_Renderer* sdlRenderer;      // ��Ⱦ��
    SDL_Texture* sdlTexture;        // ����
    SDL_Rect sdlRect;
    SDL_Event event;

    CString strFileName;
    char * chFileName;
    // ���� .yuv ������ԭʼ��Ƶ�ļ���ֱ�� SDL ��ʾ https://blog.csdn.net/leixiaohua1020/article/details/46889389
    dlg->m_browse.GetWindowTextW(strFileName);
    if (strFileName.IsEmpty()) {
        return 1;
    }
    USES_CONVERSION;
    chFileName = W2A(strFileName);

    FILE *fp = fopen(chFileName, "rb+");
    if (fp == NULL) {
        AfxMessageBox(_T("Could not open yuv file."));
        return -1;
    }

    int frameWidth, frameHeight, frameBytes;
    // ����û�б������Ϣ����֪����Ƶ֡�ߴ磬���ֱ�Ӳ��� yuv ʱ��Ҫ�ֶ�����֡�ߴ�
    frameWidth = dlg->GetDlgItemInt(IDC_EDIT_WIDTH);
    frameHeight = dlg->GetDlgItemInt(IDC_EDIT_HEIGHT);
    // YUV420P, Y:w*h  U:w*h/4  V:w*h/4
    frameBytes = frameWidth * frameHeight * 12 / 8;
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
            if (fread(buffer, 1, frameBytes, fp) != frameBytes)
                thread_exit = 1;

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


void CYuvShowDlg::OnClickedButtonPlay()
{
    // TODO: Add your control notification handler code here
    AfxBeginThread(threadFuncPlay, this);
}
