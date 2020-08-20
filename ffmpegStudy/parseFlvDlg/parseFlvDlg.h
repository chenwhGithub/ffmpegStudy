#pragma once


// CparseFlvDlg dialog


class CparseFlvDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CparseFlvDlg)

public:
	CparseFlvDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CparseFlvDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_PARSEFLV };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    void parseFlv(CString flvFileName);
    void appendTagInfo(unsigned int previousSize, unsigned char tagType, unsigned int dataSize, unsigned int timeStamp,
                       unsigned char extend, unsigned int streamId, unsigned char firstByte);
    void appendScriptInfo(unsigned int dataSize);
    CMFCEditBrowseCtrl m_browse;
    CListCtrl m_list;
    CEdit m_edit;
    FILE *m_fpFlv;
    unsigned int m_tagNum;
    CString m_editString;
    afx_msg void OnClickedButtonParseflv();
    virtual BOOL OnInitDialog();
};
