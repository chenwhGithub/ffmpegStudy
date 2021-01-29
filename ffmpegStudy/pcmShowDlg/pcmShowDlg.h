#pragma once


// CPcmShowDlg dialog

class CPcmShowDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CPcmShowDlg)

public:
	CPcmShowDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CPcmShowDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_PCMSHOW };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    CMFCEditBrowseCtrl m_browse;
    afx_msg void OnClickedButtonPlay();
    CComboBox m_combo;
    virtual BOOL OnInitDialog();
};
