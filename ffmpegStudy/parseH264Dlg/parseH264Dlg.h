#pragma once


// CparseH264Dlg dialog

#define MAXDATALEN    102400
#define MAXNALUNUM    50

typedef struct
{
    int startcodeLen;      // 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
    int dataLen;           // length of the NAL unit (Excluding the start code, which does not belong to the NALU)
    char data[MAXDATALEN]; // contains the first byte followed by the EBSP
    int forbiddenBit;      // should be always FALSE
    int referenceIdc;      // NALU_PRIORITY_xxxx
    int unitType;          // NALU_TYPE_xxxx    
    int lostPackets;       // true, if packet loss is detected
    int fileOffset;        // offset from the begin of H264 file
} NALU_t;

typedef struct
{
    int fileOffset;
    int startCodeLen;
    int dataLen;
} LenInfo_t;

class CparseH264Dlg : public CDialogEx
{
	DECLARE_DYNAMIC(CparseH264Dlg)

public:
	CparseH264Dlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CparseH264Dlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_PARSEH264 };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    BOOL getAnNalu();
    void appendNaluInfo();
    int findStartCode2(unsigned char * buf);
    int findStartCode3(unsigned char * buf);
    CMFCEditBrowseCtrl m_browse;
    CListCtrl m_list;
    CEdit m_edit;
    FILE *m_fpH264;
    NALU_t m_nalu;
    LenInfo_t m_lenInfo[MAXNALUNUM];
    int m_naluNum;

    afx_msg void OnClickedButtonParseh264();
    afx_msg void OnItemchangedList(NMHDR *pNMHDR, LRESULT *pResult);
    virtual BOOL OnInitDialog();
};
