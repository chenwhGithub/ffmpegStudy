#pragma once


// CffmpegSdlAudioDlg dialog

class CffmpegSdlAudioDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CffmpegSdlAudioDlg)

public:
	CffmpegSdlAudioDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CffmpegSdlAudioDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_FFMPEGSDL_AUDIO };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    CMFCEditBrowseCtrl m_browse;
    afx_msg void OnClickedButtonPlayAudio();
};
