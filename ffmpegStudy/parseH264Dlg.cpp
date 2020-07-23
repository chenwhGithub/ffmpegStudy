// CparseH264Dlg.cpp : implementation file
//

#include "pch.h"
#include "ffmpegStudy.h"
#include "parseH264Dlg.h"
#include "afxdialogex.h"
#include "H264_stream.h"

extern char outputstr[102400];

// CparseH264Dlg dialog

IMPLEMENT_DYNAMIC(CparseH264Dlg, CDialogEx)

CparseH264Dlg::CparseH264Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_PARSEH264, pParent)
{

}

CparseH264Dlg::~CparseH264Dlg()
{
    if (m_fpH264)
    {
        fclose(m_fpH264);
        m_fpH264 = NULL;
    }
}

void CparseH264Dlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MFCEDITBROWSE, m_browse);
    DDX_Control(pDX, IDC_LIST, m_list);
    DDX_Control(pDX, IDC_EDIT, m_edit);
}


BEGIN_MESSAGE_MAP(CparseH264Dlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_PARSEH264, &CparseH264Dlg::OnClickedButtonParseh264)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST, &CparseH264Dlg::OnItemchangedList)
END_MESSAGE_MAP()


// CparseH264Dlg message handlers


BOOL CparseH264Dlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // TODO:  Add extra initialization here
    m_fpH264 = NULL;
    m_naluNum = 0;

    DWORD dwExStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_HEADERDRAGDROP | LVS_EX_ONECLICKACTIVATE;
    m_list.ModifyStyle(0, LVS_SINGLESEL | LVS_REPORT | LVS_SHOWSELALWAYS);
    m_list.SetExtendedStyle(dwExStyle);
    m_list.InsertColumn(0, _T("num"), LVCFMT_CENTER, 50, 0);
    m_list.InsertColumn(1, _T("forbiddenBit"), LVCFMT_CENTER, 100, 0);
    m_list.InsertColumn(2, _T("referenceIdc"), LVCFMT_CENTER, 100, 0);
    m_list.InsertColumn(3, _T("naluType"), LVCFMT_CENTER, 100, 0);
    m_list.InsertColumn(4, _T("startCodeLen"), LVCFMT_CENTER, 100, 0);
    m_list.InsertColumn(5, _T("dataLen"), LVCFMT_CENTER, 100, 0);
    m_list.InsertColumn(6, _T("fileOffset"), LVCFMT_CENTER, 100, 0);

    m_browse.EnableFileBrowseButton(NULL, _T("H.264 Files (*.264,*.h264)|*.264;*.h264|All Files (*.*)|*.*||"));

    return TRUE;  // return TRUE unless you set the focus to a control
                  // EXCEPTION: OCX Property Pages should return FALSE
}


void CparseH264Dlg::OnClickedButtonParseh264()
{
    // TODO: Add your control notification handler code here
    m_naluNum = 0;
    m_list.DeleteAllItems();
    m_edit.Clear();
    if (m_fpH264)
    {
        fclose(m_fpH264);
        m_fpH264 = NULL;
    }

    CString fileName;
    BOOL getNaluSuccess = TRUE;
    int fileOffset = 0;

    m_browse.GetWindowTextW(fileName);
    if (fileName.IsEmpty())
    {
        return;
    }

    USES_CONVERSION;
    m_fpH264 = fopen(W2A(fileName), "r+b");
    while (!feof(m_fpH264))
    {
        getNaluSuccess = getAnNalu();
        if ((!getNaluSuccess) || (m_naluNum == MAXNALUNUM))
        {
            break;
        }

        m_nalu.fileOffset = fileOffset;
        fileOffset = fileOffset + m_nalu.startcodeLen + m_nalu.dataLen;

        m_lenInfo[m_naluNum].fileOffset = m_nalu.fileOffset;
        m_lenInfo[m_naluNum].startCodeLen = m_nalu.startcodeLen;
        m_lenInfo[m_naluNum].dataLen = m_nalu.dataLen;

        appendNaluInfo();

        m_naluNum++;
    }
}


BOOL CparseH264Dlg::getAnNalu() // 每次执行，文件的指针指向本次找到的NALU的末尾，下一个位置即为下个NALU的起始码
{
    int startCode2Flag = 0, startCode3Flag = 0, nextStartCodeFound = 0;
    int pos = 0, rewind = 0;
    static unsigned char buf[MAXDATALEN];

    fread(buf, 1, 3, m_fpH264); // 从码流中读3个字节
    startCode2Flag = findStartCode2(buf); // 判断是否为 0x000001
    if (!startCode2Flag)
    {
        fread(buf + 3, 1, 1, m_fpH264); // 如果不是 0x000001, 再读一个字节
        startCode3Flag = findStartCode3(buf); // 判断是否为 0x00000001
        if (!startCode3Flag) // 如果不是 0x00000001, 返回 FALSE
        {
            return FALSE;
        }
        else
        {
            pos = 4; // 如果是 0x00000001, 设置 startCodeLen 为4个字节
            m_nalu.startcodeLen = 4;
        }
    }
    else
    {
        pos = 3; // 如果是 0x000001, 设置 startCodeLen 为3个字节
        m_nalu.startcodeLen = 3;
    }

    //查找下一个 startCode
    nextStartCodeFound = 0;
    startCode2Flag = 0;
    startCode3Flag = 0;
    while (!nextStartCodeFound)
    {
        if (feof(m_fpH264)) // 到了文件尾
        {
            m_nalu.dataLen = (pos - 1) - m_nalu.startcodeLen;
            memcpy(m_nalu.data, &buf[m_nalu.startcodeLen], m_nalu.dataLen);
            m_nalu.forbiddenBit = (m_nalu.data[0] & 0x80) >> 7; // 1000 0000
            m_nalu.referenceIdc = (m_nalu.data[0] & 0x60) >> 5; // 0110 0000
            m_nalu.unitType = m_nalu.data[0] & 0x1f; // 0001 1111
            return TRUE;
        }

        buf[pos++] = fgetc(m_fpH264); // 读一个字节到BUF中
        startCode3Flag = findStartCode3(&buf[pos - 4]); // 判断是否为 0x00000001
        if (!startCode3Flag)
        {
            startCode2Flag = findStartCode2(&buf[pos - 3]); // 判断是否为 0x000001
        }
        nextStartCodeFound = (startCode2Flag || startCode3Flag);
    }

    rewind = (startCode3Flag == 1) ? -4 : -3;
    fseek(m_fpH264, rewind, SEEK_CUR); // 把文件指针指向当前NALU的末尾
    m_nalu.dataLen = (pos + rewind) - m_nalu.startcodeLen;
    memcpy(m_nalu.data, &buf[m_nalu.startcodeLen], m_nalu.dataLen);
    m_nalu.forbiddenBit = (m_nalu.data[0] & 0x80) >> 7; // 1000 0000
    m_nalu.referenceIdc = (m_nalu.data[0] & 0x60) >> 5; // 0110 0000
    m_nalu.unitType = m_nalu.data[0] & 0x1f; // 0001 1111
    return TRUE;
}


int CparseH264Dlg::findStartCode2(unsigned char *buf)
{
    if (buf[0] != 0 || buf[1] != 0 || buf[2] != 1) return 0; // 判断是否为 0x000001, 如果是返回1
    else return 1;
}


int CparseH264Dlg::findStartCode3(unsigned char *buf)
{
    if (buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 1) return 0; // 判断是否为 0x00000001, 如果是返回1
    else return 1;
}


void CparseH264Dlg::appendNaluInfo()
{
    CString strIndex, strForbiddenBit, strReferenceIdc, strUnitType, strStartCodeLen, strDataLen, strFileOffset;

    strIndex.Format(_T("%d"), m_naluNum);

    strForbiddenBit.Format(_T("%d"), m_nalu.forbiddenBit);

    switch (m_nalu.referenceIdc)
    {
    case 0:strReferenceIdc.Format(_T("DISPOS")); break;
    case 1:strReferenceIdc.Format(_T("LOW")); break;
    case 2:strReferenceIdc.Format(_T("HIGH")); break;
    case 3:strReferenceIdc.Format(_T("HIGHEST")); break;
    default:strReferenceIdc.Format(_T("Other")); break;
    }

    switch (m_nalu.unitType)
    {
    case 1:strUnitType.Format(_T("SLICE")); break;
    case 2:strUnitType.Format(_T("DPA")); break;
    case 3:strUnitType.Format(_T("DPB")); break;
    case 4:strUnitType.Format(_T("DPC")); break;
    case 5:strUnitType.Format(_T("IDR_SLICE")); break;
    case 6:strUnitType.Format(_T("SEI")); break;
    case 7:strUnitType.Format(_T("SPS")); break;
    case 8:strUnitType.Format(_T("PPS")); break;
    case 9:strUnitType.Format(_T("AUD")); break;
    case 10:strUnitType.Format(_T("END_SEQUENCE")); break;
    case 11:strUnitType.Format(_T("END_STREAM")); break;
    case 12:strUnitType.Format(_T("FILLER_DATA")); break;
    case 13:strUnitType.Format(_T("SPS_EXT")); break;
    case 19:strUnitType.Format(_T("AUXILIARY_SLICE")); break;
    default:strUnitType.Format(_T("Other")); break;
    }

    strStartCodeLen.Format(_T("%d"), m_nalu.startcodeLen);
    strDataLen.Format(_T("%d"), m_nalu.dataLen);
    strFileOffset.Format(_T("%d"), m_nalu.fileOffset);

    m_list.InsertItem(m_naluNum, strIndex);
    m_list.SetItemText(m_naluNum, 1, strForbiddenBit);
    m_list.SetItemText(m_naluNum, 2, strReferenceIdc);
    m_list.SetItemText(m_naluNum, 3, strUnitType);
    m_list.SetItemText(m_naluNum, 4, strStartCodeLen);
    m_list.SetItemText(m_naluNum, 5, strDataLen);
    m_list.SetItemText(m_naluNum, 6, strFileOffset);
}


void CparseH264Dlg::OnItemchangedList(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    // TODO: Add your control notification handler code here

    int selectedIndex = 0, naluFileOffset = 0, naluStartCodeLen = 0, naluDataLen = 0;
    static unsigned char naluData[MAXDATALEN] = { 0 };
    h264_stream_t* h = NULL;

    memset(outputstr, 0, sizeof(outputstr));
    selectedIndex = m_list.GetNextItem(-1, LVIS_SELECTED);
    naluFileOffset = m_lenInfo[selectedIndex].fileOffset;
    naluStartCodeLen = m_lenInfo[selectedIndex].startCodeLen;
    naluDataLen = m_lenInfo[selectedIndex].dataLen;

    fseek(m_fpH264, naluFileOffset + naluStartCodeLen, SEEK_SET); // read NALU data without startCode
    fread(naluData, naluDataLen, 1, m_fpH264);

    h = h264_new();
    read_nal_unit(h, naluData, naluDataLen); // parse NALU IE based on nalType
    debug_nal(h, h->nal);
    h264_free(h);

    CString strNaluInfo = CString(outputstr);
    m_edit.SetWindowTextW(strNaluInfo);

    *pResult = 0;
}
