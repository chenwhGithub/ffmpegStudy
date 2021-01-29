#pragma once


// CDecodeShowVideoDlg dialog

class CDecodeShowVideoDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CDecodeShowVideoDlg)

public:
    CDecodeShowVideoDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CDecodeShowVideoDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_DECODESHOWVIDEO };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    CMFCEditBrowseCtrl m_browse;
    afx_msg void OnClickedButtonPlay();
    int decodeH264ToYuv(const char* inFileName, const char* outFileName);
};
