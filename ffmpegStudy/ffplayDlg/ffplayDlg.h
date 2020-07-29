#pragma once


// CffplayDlg dialog
#define BUTTON_MODE_START   0
#define BUTTON_MODE_PAUSE   1
#define BUTTON_MODE_RESUME  2

class CffplayDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CffplayDlg)

public:
	CffplayDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CffplayDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_FFMPEGSDL_FFPLAY };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnClickedButtonStartpause();
    afx_msg void OnClickedButtonSeekprev();
    afx_msg void OnClickedButtonSeeknext();
    afx_msg void OnClickedButtonStop();
    afx_msg void OnClickedButtonFrame();
    afx_msg void OnClickedButtonFullscreen();
    virtual BOOL OnInitDialog();
    CMFCEditBrowseCtrl m_browse;
    CStatic m_show;
    CSliderCtrl m_slider;
    CStatic m_duration;
    int m_btnMode; // 0:start, 1:pause, 2:resume
};
