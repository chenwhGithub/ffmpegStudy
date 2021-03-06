
// ffmpegStudyDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "ffmpegStudy.h"
#include "ffmpegStudyDlg.h"
#include "afxdialogex.h"
#include "parseFlvDlg/parseFlvDlg.h"
#include "parseH264Dlg/parseH264Dlg.h"
#include "ffmpegSdlVideoDlg/ffmpegSdlVideoDlg.h"
#include "ffmpegSdlAudioDlg/ffmpegSdlAudioDlg.h"
#include "ffplayDlg/ffplayDlg.h"
#include "yuvShowDlg/yuvShowDlg.h"
#include "decodeShowVideoDlg/decodeShowVideoDlg.h"
#include "pcmShowDlg/pcmShowDlg.h"
#include "decodeShowAudioDlg/decodeShowAudioDlg.h"
#include "encodeYuvToH264Dlg/encodeYuvToH264Dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CffmpegStudyDlg dialog



CffmpegStudyDlg::CffmpegStudyDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_FFMPEGSTUDY_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CffmpegStudyDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CffmpegStudyDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BUTTON_PARSEH264, &CffmpegStudyDlg::OnClickedButtonParseh264)
    ON_BN_CLICKED(IDC_BUTTON_PARSEAAC, &CffmpegStudyDlg::OnClickedButtonParseaac)
    ON_BN_CLICKED(IDC_BUTTON_FFMPEGVIDEO, &CffmpegStudyDlg::OnClickedButtonFfmpegvideo)
    ON_BN_CLICKED(IDC_BUTTON_FFMPEGAUDIO, &CffmpegStudyDlg::OnClickedButtonFfmpegaudio)
    ON_BN_CLICKED(IDC_BUTTON_FFMPEGFFPLAY, &CffmpegStudyDlg::OnClickedButtonFfmpegFfplay)
    ON_BN_CLICKED(IDC_BUTTON_PARSEFLV, &CffmpegStudyDlg::OnClickedButtonParseflv)
    ON_BN_CLICKED(IDC_BUTTON_YUVSHOW, &CffmpegStudyDlg::OnClickedButtonYuvshow)
    ON_BN_CLICKED(IDC_BUTTON_DECODESHOWVIDEO, &CffmpegStudyDlg::OnClickedButtonDecodeshowvideo)
    ON_BN_CLICKED(IDC_BUTTON_PCMSHOW, &CffmpegStudyDlg::OnClickedButtonPcmshow)
    ON_BN_CLICKED(IDC_BUTTON_DECODESHOWAUDIO, &CffmpegStudyDlg::OnClickedButtonDecodeshowaudio)
    ON_BN_CLICKED(IDC_BUTTON_YUVTOH264, &CffmpegStudyDlg::OnClickedButtonYuvtoh264)
END_MESSAGE_MAP()


// CffmpegStudyDlg message handlers

BOOL CffmpegStudyDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CffmpegStudyDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CffmpegStudyDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


// parse H264 file without FFmpeg lib
void CffmpegStudyDlg::OnClickedButtonParseh264()
{
    // TODO: Add your control notification handler code here
    CparseH264Dlg dlg;
    dlg.DoModal();
}


// parse AAC file without FFmpeg lib
void CffmpegStudyDlg::OnClickedButtonParseaac()
{
    // TODO: Add your control notification handler code here
    CString temp = CString("TODO");
    MessageBox(temp);
}


// parse video file with FFmpeg lib
void CffmpegStudyDlg::OnClickedButtonFfmpegvideo()
{
    // TODO: Add your control notification handler code here
    CffmpegSdlVideoDlg dlg;
    dlg.DoModal();
}


void CffmpegStudyDlg::OnClickedButtonFfmpegaudio()
{
    // TODO: Add your control notification handler code here
    CffmpegSdlAudioDlg dlg;
    dlg.DoModal();
}


void CffmpegStudyDlg::OnClickedButtonFfmpegFfplay()
{
    // TODO: Add your control notification handler code here
    CffplayDlg dlg;
    dlg.DoModal();
}


void CffmpegStudyDlg::OnClickedButtonParseflv()
{
    // TODO: Add your control notification handler code here
    CparseFlvDlg dlg;
    dlg.DoModal();
}


void CffmpegStudyDlg::OnClickedButtonYuvshow()
{
    // TODO: Add your control notification handler code here
    CYuvShowDlg dlg;
    dlg.DoModal();
}


void CffmpegStudyDlg::OnClickedButtonDecodeshowvideo()
{
    // TODO: Add your control notification handler code here
    CDecodeShowVideoDlg dlg;
    dlg.DoModal();
}


void CffmpegStudyDlg::OnClickedButtonPcmshow()
{
    // TODO: Add your control notification handler code here
    CPcmShowDlg dlg;
    dlg.DoModal();
}


void CffmpegStudyDlg::OnClickedButtonDecodeshowaudio()
{
    // TODO: Add your control notification handler code here
    CDecodeShowAudioDlg dlg;
    dlg.DoModal();
}


void CffmpegStudyDlg::OnClickedButtonYuvtoh264()
{
    // TODO: Add your control notification handler code here
    CEncodeYuvToH264Dlg dlg;
    dlg.DoModal();
}
