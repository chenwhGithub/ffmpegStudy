// parseFlvDlg.cpp : implementation file
//

#include "../pch.h"
#include "../ffmpegStudy.h"
#include "parseFlvDlg.h"
#include "afxdialogex.h"

#pragma pack(1)
typedef struct {
    unsigned char signature[3];
    unsigned char version;
    unsigned char flags;
    unsigned char dataOffset[4];
} FLV_HEADER;

typedef struct {
    unsigned char tagType;
    unsigned char dataSize[3];
    unsigned char timeStamp[3];
    unsigned char extend;
    unsigned char streamId[3];
} TAG_HEADER;
#pragma pack()

// CparseFlvDlg dialog

IMPLEMENT_DYNAMIC(CparseFlvDlg, CDialogEx)

CparseFlvDlg::CparseFlvDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_PARSEFLV, pParent)
{

}

CparseFlvDlg::~CparseFlvDlg()
{
    if (m_fpFlv)
    {
        fclose(m_fpFlv);
        m_fpFlv = NULL;
    }
}

void CparseFlvDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MFCEDITBROWSE, m_browse);
    DDX_Control(pDX, IDC_LIST, m_list);
    DDX_Control(pDX, IDC_EDIT, m_edit);
}


BEGIN_MESSAGE_MAP(CparseFlvDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_PARSEFLV, &CparseFlvDlg::OnClickedButtonParseflv)
END_MESSAGE_MAP()


// CparseFlvDlg message handlers


BOOL CparseFlvDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // TODO:  Add extra initialization here
    m_fpFlv = NULL;
    m_tagNum = 0;

    DWORD dwExStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_HEADERDRAGDROP | LVS_EX_ONECLICKACTIVATE;
    m_list.ModifyStyle(0, LVS_SINGLESEL | LVS_REPORT | LVS_SHOWSELALWAYS);
    m_list.SetExtendedStyle(dwExStyle);
    m_list.InsertColumn(0, _T("num"), LVCFMT_CENTER, 50, 0);
    m_list.InsertColumn(1, _T("previousSize"), LVCFMT_CENTER, 100, 0);
    m_list.InsertColumn(2, _T("type"), LVCFMT_CENTER, 100, 0);
    m_list.InsertColumn(3, _T("dataSize"), LVCFMT_CENTER, 100, 0);
    m_list.InsertColumn(4, _T("timeStamp"), LVCFMT_CENTER, 100, 0);
    m_list.InsertColumn(5, _T("extend"), LVCFMT_CENTER, 50, 0);
    m_list.InsertColumn(6, _T("steamId"), LVCFMT_CENTER, 70, 0);
    m_list.InsertColumn(7, _T("tagData1stByte"), LVCFMT_CENTER, 200, 0);

    m_browse.EnableFileBrowseButton(NULL, _T("flv Files (*.flv)|*.flv|All Files (*.*)|*.*||"));

    return TRUE;  // return TRUE unless you set the focus to a control
                    // EXCEPTION: OCX Property Pages should return FALSE
}


void CparseFlvDlg::OnClickedButtonParseflv()
{
    // TODO: Add your control notification handler code here
    m_list.DeleteAllItems();
    m_edit.Clear();
    m_editString.Empty();
    m_tagNum = 0;
    if (m_fpFlv)
    {
        fclose(m_fpFlv);
        m_fpFlv = NULL;
    }

    CString fileName;
    m_browse.GetWindowTextW(fileName);
    if (fileName.IsEmpty())
    {
        return;
    }
    parseFlv(fileName);
}


unsigned int CparseFlvDlg::reverseInt(unsigned char *bytes)
{
    unsigned int r = 0;
    r = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
    return r;
}


void CparseFlvDlg::parseFlv(CString flvFileName)
{
    FLV_HEADER flvHeader;
    TAG_HEADER tagHeader;
    CString str;
    unsigned char chPreviousSize[4] = { 0 };
    unsigned int previousSize = 0;
    unsigned char tagType = 0;
    unsigned int dataSize = 0;
    unsigned int timeStamp = 0;
    unsigned char extend = 0;
    unsigned int streamId = 0;
    unsigned char firstByte = 0;

    USES_CONVERSION;
    m_fpFlv = fopen(W2A(flvFileName), "r+b");

    fread((unsigned char *)&flvHeader, 1, sizeof(FLV_HEADER), m_fpFlv);
    m_editString.Format(_T("**************** Header ****************\r\n"));
    str.Format(_T("signature: 0x %X %X %X\r\n"), flvHeader.signature[0], flvHeader.signature[1], flvHeader.signature[2]); // "F L V"
    m_editString += str;
    str.Format(_T("version: 0x %X\r\n"), flvHeader.version); // 0x1
    m_editString += str;
    str.Format(_T("flags: 0x %X\r\n"), flvHeader.flags);   // 0x5
    m_editString += str;
    str.Format(_T("offset: 0x %X\r\n"), (flvHeader.dataOffset[0] << 24) | (flvHeader.dataOffset[1] << 16) | (flvHeader.dataOffset[2] << 8) | flvHeader.dataOffset[3]); // 0x9
    m_editString += str;

    fread(chPreviousSize, sizeof(chPreviousSize), 1, m_fpFlv); // tag0 size
    previousSize = reverseInt(chPreviousSize);
    while (!feof(m_fpFlv))
    {
         fread((unsigned char *)&tagHeader, sizeof(TAG_HEADER), 1, m_fpFlv);
         tagType = tagHeader.tagType;
         dataSize = (tagHeader.dataSize[0] << 16) | (tagHeader.dataSize[1] << 8) | tagHeader.dataSize[2];
         timeStamp = (tagHeader.timeStamp[0] << 16) | (tagHeader.timeStamp[1] << 8) | tagHeader.timeStamp[2];
         extend = tagHeader.extend;
         streamId = (tagHeader.streamId[0] << 16) | (tagHeader.streamId[1] << 8) | tagHeader.streamId[2];
         switch (tagType)
         {
         case TAG_TYPE_AUDIO:
         case TAG_TYPE_VIDEO:
             fread((unsigned char *)&firstByte, sizeof(firstByte), 1, m_fpFlv);
             appendTagInfo(previousSize, tagType, dataSize, timeStamp, extend, streamId, firstByte);
             fseek(m_fpFlv, dataSize - 1, SEEK_CUR);
             break;
         case TAG_TYPE_SCRIPT:
             appendTagInfo(previousSize, tagType, dataSize, timeStamp, extend, streamId, 0);
             fseek(m_fpFlv, dataSize, SEEK_CUR);
             break;
         default:
             fseek(m_fpFlv, dataSize, SEEK_CUR);
             break;
         }
         fread(chPreviousSize, sizeof(chPreviousSize), 1, m_fpFlv); // tag1-N size
         previousSize = reverseInt(chPreviousSize);

         m_tagNum++;
         if (TAG_MAXNUM == m_tagNum)
         {
             break;
         }
    }

    m_edit.SetWindowTextW(m_editString);
}


void CparseFlvDlg::appendTagInfo(unsigned int previousSize, unsigned char tagType, unsigned int dataSize, unsigned int timeStamp,
                                 unsigned char extend, unsigned int streamId, unsigned char firstByte)
{
    CString strIndex, strPreviousSize, strTagType, strDataSize, strTimeStamp, strExtend, strStreamId, strFirstByte;
    int mSec = timeStamp % 1000;
    int sec = (timeStamp / 1000) % 60;
    int min = ((timeStamp / 1000) / 60) % 60;
    int hour = (timeStamp / 1000) / 3600;

    strIndex.Format(_T("%d"), m_tagNum);
    strPreviousSize.Format(_T("%d"), previousSize);
    strDataSize.Format(_T("%d"), dataSize);
    strTimeStamp.Format(_T("%02d:%02d:%02d.%03d"), hour, min, sec, mSec);
    strExtend.Format(_T("%d"), extend);
    strStreamId.Format(_T("%d"), streamId);

    switch (tagType)
    {
    case TAG_TYPE_AUDIO:
        strTagType.Format(_T("Audio"));

        switch ((firstByte & 0xF0) >> 4)
        {
        case 0:strFirstByte.AppendFormat(_T("Linear PCM, platform endian")); break;
        case 1:strFirstByte.AppendFormat(_T("ADPCM")); break;
        case 2:strFirstByte.AppendFormat(_T("MP3")); break;
        case 3:strFirstByte.AppendFormat(_T("Linear PCM, little endian")); break;
        case 4:strFirstByte.AppendFormat(_T("Nellymoser 16-kHz mono")); break;
        case 5:strFirstByte.AppendFormat(_T("Nellymoser 8-kHz mono")); break;
        case 6:strFirstByte.AppendFormat(_T("Nellymoser")); break;
        case 7:strFirstByte.AppendFormat(_T("G.711 A-law logarithmic PCM")); break;
        case 8:strFirstByte.AppendFormat(_T("G.711 mu-law logarithmic PCM")); break;
        case 9:strFirstByte.AppendFormat(_T("reserved")); break;
        case 10:strFirstByte.AppendFormat(_T("AAC")); break;
        case 11:strFirstByte.AppendFormat(_T("Speex")); break;
        case 14:strFirstByte.AppendFormat(_T("MP3 8-Khz")); break;
        case 15:strFirstByte.AppendFormat(_T("Device-specific sound")); break;
        default: break;
        }

        strFirstByte.AppendFormat(_T("|"));
        switch ((firstByte & 0x0C) >> 2)
        {
        case 0:strFirstByte.AppendFormat(_T("5.5-kHz")); break;
        case 1:strFirstByte.AppendFormat(_T("11-kHz")); break;
        case 2:strFirstByte.AppendFormat(_T("22-kHz")); break;
        case 3:strFirstByte.AppendFormat(_T("44-kHz")); break;
        default: break;
        }

        strFirstByte.AppendFormat(_T("|"));
        switch ((firstByte & 0x02) >> 1)
        {
        case 0:strFirstByte.AppendFormat(_T("8Bit")); break;
        case 1:strFirstByte.AppendFormat(_T("16Bit")); break;
        default: break;
        }

        strFirstByte.AppendFormat(_T("|"));
        switch (firstByte & 0x01)
        {
        case 0:strFirstByte.AppendFormat(_T("Mono")); break;
        case 1:strFirstByte.AppendFormat(_T("Stereo")); break;
        default: break;
        }
        break;
    case TAG_TYPE_VIDEO:
        strTagType.Format(_T("Video"));

        switch ((firstByte & 0xF0) >> 4)
        {
        case 1:strFirstByte.AppendFormat(_T("key frame")); break;
        case 2:strFirstByte.AppendFormat(_T("inter frame")); break;
        case 3:strFirstByte.AppendFormat(_T("disposable inter frame")); break;
        case 4:strFirstByte.AppendFormat(_T("generated keyframe")); break;
        case 5:strFirstByte.AppendFormat(_T("video info/command frame")); break;
        default:strFirstByte.AppendFormat(_T("%s"), "Unknown"); break;
        }

        strFirstByte.AppendFormat(_T("|"));
        switch ((firstByte & 0x0F))
        {
        case 1:strFirstByte.AppendFormat(_T("JPEG")); break;
        case 2:strFirstByte.AppendFormat(_T("Sorenson H.263")); break;
        case 3:strFirstByte.AppendFormat(_T("Screen video")); break;
        case 4:strFirstByte.AppendFormat(_T("On2 VP6")); break;
        case 5:strFirstByte.AppendFormat(_T("On2 VP6 with alpha channel")); break;
        case 6:strFirstByte.AppendFormat(_T("Screen video version 2")); break;
        case 7:strFirstByte.AppendFormat(_T("AVC")); break;
        default:strFirstByte.AppendFormat(_T("%s"), "Unknown"); break;
        }
        break;
    case TAG_TYPE_SCRIPT:
        strTagType.Format(_T("Script"));
        break;
    default:
        strTagType.Format(_T("Unknown"));
        break;
    }

    m_list.InsertItem(m_tagNum, strIndex);
    m_list.SetItemText(m_tagNum, 1, strPreviousSize);
    m_list.SetItemText(m_tagNum, 2, strTagType);
    m_list.SetItemText(m_tagNum, 3, strDataSize);
    m_list.SetItemText(m_tagNum, 4, strTimeStamp);
    m_list.SetItemText(m_tagNum, 5, strExtend);
    m_list.SetItemText(m_tagNum, 6, strStreamId);
    m_list.SetItemText(m_tagNum, 7, strFirstByte);
}