#pragma once


// CDecodeShowAudioDlg dialog

class CDecodeShowAudioDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CDecodeShowAudioDlg)

public:
	CDecodeShowAudioDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CDecodeShowAudioDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_DECODESHOWAUDIO };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    CMFCEditBrowseCtrl m_browse;
    afx_msg void OnClickedButtonPlay();
};
