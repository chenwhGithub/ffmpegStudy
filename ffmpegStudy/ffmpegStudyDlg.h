
// ffmpegStudyDlg.h : header file
//

#pragma once


// CffmpegStudyDlg dialog
class CffmpegStudyDlg : public CDialogEx
{
// Construction
public:
	CffmpegStudyDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FFMPEGSTUDY_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnClickedButtonParseh264();
    afx_msg void OnClickedButtonParseaac();
    afx_msg void OnClickedButtonFfmpegvideo();
    afx_msg void OnClickedButtonFfmpegaudio();
    afx_msg void OnClickedButtonFfmpegFfplay();
    afx_msg void OnClickedButtonParseflv();
    afx_msg void OnClickedButtonYuvshow();
    afx_msg void OnClickedButtonDecodeshow();
};
