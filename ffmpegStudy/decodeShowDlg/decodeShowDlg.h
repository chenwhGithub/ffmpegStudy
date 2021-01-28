#pragma once


// CDecodeShowDlg dialog

class CDecodeShowDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CDecodeShowDlg)

public:
	CDecodeShowDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CDecodeShowDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_DECODESHOW };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    CMFCEditBrowseCtrl m_browse;
    afx_msg void OnClickedButtonPlay();
    int decodeH264ToYuv(const char* inFileName, const char* outFileName);
};
