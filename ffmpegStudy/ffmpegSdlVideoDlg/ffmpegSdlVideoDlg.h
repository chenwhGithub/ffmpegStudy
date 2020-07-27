#pragma once


// CffmpegSdlVideoDlg dialog

class CffmpegSdlVideoDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CffmpegSdlVideoDlg)

public:
	CffmpegSdlVideoDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CffmpegSdlVideoDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_FFMPEGSDL_VIDEO };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    CMFCEditBrowseCtrl m_browse;
    CEdit m_edit;
    afx_msg void OnClickedButtonPlayVideo();
};
