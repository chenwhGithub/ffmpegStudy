// C:\code\H264\ffmpegStudy\ffmpegStudy\ffplayDlg\ffplayDlg.cpp : implementation file
//

#include "../pch.h"
#include "../ffmpegStudy.h"
#include "ffplayDlg.h"
#include "afxdialogex.h"
#include "../libs/ffplay/ffplay.h"

IMPLEMENT_DYNAMIC(CffplayDlg, CDialogEx)

CffplayDlg::CffplayDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_FFMPEGSDL_FFPLAY, pParent)
{

}

CffplayDlg::~CffplayDlg()
{
}

void CffplayDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MFCEDITBROWSE, m_browse);
    DDX_Control(pDX, IDC_STATIC_SHOW, m_show);
    DDX_Control(pDX, IDC_SLIDER_PROCESS, m_slider);
    DDX_Control(pDX, IDC_STATIC_DURATION, m_duration);
}


BEGIN_MESSAGE_MAP(CffplayDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_STARTPAUSE, &CffplayDlg::OnClickedButtonStartpause)
    ON_BN_CLICKED(IDC_BUTTON_SEEKPREV, &CffplayDlg::OnClickedButtonSeekprev)
    ON_BN_CLICKED(IDC_BUTTON_SEEKNEXT, &CffplayDlg::OnClickedButtonSeeknext)
    ON_BN_CLICKED(IDC_BUTTON_STOP, &CffplayDlg::OnClickedButtonStop)
    ON_BN_CLICKED(IDC_BUTTON_FRAME, &CffplayDlg::OnClickedButtonFrame)
    ON_BN_CLICKED(IDC_BUTTON_FULLSCREEN, &CffplayDlg::OnClickedButtonFullscreen)
END_MESSAGE_MAP()


// CffplayDlg message handlers
BOOL CffplayDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // TODO:  Add extra initialization here
    m_btnMode = BUTTON_MODE_START;

    GetDlgItem(IDC_BUTTON_SEEKPREV)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_SEEKNEXT)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_FRAME)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_FULLSCREEN)->EnableWindow(FALSE);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // EXCEPTION: OCX Property Pages should return FALSE
}


void CffplayDlg::OnClickedButtonStartpause()
{
    // TODO: Add your control notification handler code here
    if (m_btnMode == BUTTON_MODE_START)  // ��ʼ����
    {
        AfxBeginThread(ffplay_start, this);

        SetDlgItemTextW(IDC_BUTTON_STARTPAUSE, CString("Pause"));
        GetDlgItem(IDC_BUTTON_SEEKPREV)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_SEEKNEXT)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_FRAME)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_FULLSCREEN)->EnableWindow(TRUE);
        m_btnMode = BUTTON_MODE_PAUSE;
    }
    else
    {
        SDL_Event event;
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = SDLK_SPACE;
        SDL_PushEvent(&event);

        if (m_btnMode == BUTTON_MODE_PAUSE)
        {
            SetDlgItemTextW(IDC_BUTTON_STARTPAUSE, CString("Resume"));
            m_btnMode = BUTTON_MODE_RESUME;
        }
        else
        {
            SetDlgItemTextW(IDC_BUTTON_STARTPAUSE, CString("Pause"));
            m_btnMode = BUTTON_MODE_PAUSE;
        }
    }
}


void CffplayDlg::OnClickedButtonSeekprev()
{
    // TODO: Add your control notification handler code here
}


void CffplayDlg::OnClickedButtonSeeknext()
{
    // TODO: Add your control notification handler code here
}


void CffplayDlg::OnClickedButtonStop()
{
    // TODO: Add your control notification handler code here
/*    SDL_Event event;
    event.type = FF_QUIT_EVENT;
    SDL_PushEvent(&event);

    GetDlgItem(IDC_BUTTON_SEEKPREV)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_SEEKNEXT)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_FRAME)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_FULLSCREEN)->EnableWindow(FALSE);
    m_btnMode == BUTTON_MODE_START;
*/
}


void CffplayDlg::OnClickedButtonFrame()
{
    // TODO: Add your control notification handler code here
}


void CffplayDlg::OnClickedButtonFullscreen()
{
    // TODO: Add your control notification handler code here
    SDL_Event event;
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_f;
    SDL_PushEvent(&event);
}
