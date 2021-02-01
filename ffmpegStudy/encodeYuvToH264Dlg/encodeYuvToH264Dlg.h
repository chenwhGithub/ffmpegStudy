#pragma once


// CEncodeYuvToH264Dlg dialog

class CEncodeYuvToH264Dlg : public CDialogEx
{
	DECLARE_DYNAMIC(CEncodeYuvToH264Dlg)

public:
	CEncodeYuvToH264Dlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CEncodeYuvToH264Dlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_YUVTOH264 };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    CMFCEditBrowseCtrl m_browse;
    afx_msg void OnClickedButtonPlay();
};
