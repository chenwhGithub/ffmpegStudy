#pragma once


// CYuvShowDlg dialog

class CYuvShowDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CYuvShowDlg)

public:
	CYuvShowDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CYuvShowDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_YUVSHOW };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    CMFCEditBrowseCtrl m_browse;
    afx_msg void OnClickedButtonPlay();
};
