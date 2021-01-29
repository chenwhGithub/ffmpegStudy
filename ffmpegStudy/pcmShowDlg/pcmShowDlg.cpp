// pcmShowDlg.cpp : implementation file
//

#include "../pch.h"
#include "../ffmpegStudy.h"
#include "pcmShowDlg.h"
#include "afxdialogex.h"

#include "SDL.h"

static Uint8 *audio_chunk;      // һ֡��Ƶ�������ݻ�����
static Uint32 audio_len;        // �������ݻ�����ʣ��δ���ŵĳ���
static Uint8 *audio_pos;        // �������ݻ�������ǰ���ŵ�λ��

// ��Ƶ�豸��Ҫ�������ݵ�ʱ�����øûص�������Ӧ�ó��򱻶��������ݵ���Ƶ�豸
static void fill_audio(void *udata, Uint8 *stream, int len)
{
    SDL_memset(stream, 0, len);
    if (audio_len == 0)
        return;
    len = (len > audio_len ? audio_len : len);

    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
}


// CPcmShowDlg dialog

IMPLEMENT_DYNAMIC(CPcmShowDlg, CDialogEx)

CPcmShowDlg::CPcmShowDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_PCMSHOW, pParent)
{

}

CPcmShowDlg::~CPcmShowDlg()
{
}

void CPcmShowDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MFCEDITBROWSE, m_browse);
    DDX_Control(pDX, IDC_COMBO_SAMPLEFORMAT, m_combo);
}


BEGIN_MESSAGE_MAP(CPcmShowDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_PLAY, &CPcmShowDlg::OnClickedButtonPlay)
END_MESSAGE_MAP()


BOOL CPcmShowDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // TODO:  Add extra initialization here
    m_combo.InsertString(0, _T("U16"));
    m_combo.InsertString(1, _T("S16"));
    m_combo.InsertString(2, _T("S32"));
    m_combo.InsertString(3, _T("F32"));

    m_combo.SetCurSel(3);
    return TRUE;  // return TRUE unless you set the focus to a control
                  // EXCEPTION: OCX Property Pages should return FALSE
}


// CPcmShowDlg message handlers
static UINT threadFuncPlay(LPVOID lpParam)
{
    CPcmShowDlg *dlg = (CPcmShowDlg *)lpParam;

    CString strInFileName;
    char * chInFileName;
    // ���� flv, mp4, mp3, aac �ȴ���Ƶ���ļ������� ���װ->����->pcm->���� https://blog.csdn.net/leixiaohua1020/article/details/46890259
    dlg->m_browse.GetWindowTextW(strInFileName);
    if (strInFileName.IsEmpty()) {
        return 1;
    }
    USES_CONVERSION;
    chInFileName = W2A(strInFileName);

    FILE *fp = fopen(chInFileName, "rb");
    if (fp == NULL) {
        AfxMessageBox(_T("Couldn't open file."));
        return 1;
    }

    const int frameSize = 1024;
    int channels, sampleBytes, sampleRate, format, bufferSize;
    unsigned char *buffer;

    sampleRate = dlg->GetDlgItemInt(IDC_EDIT_SAMPLERATE);
    channels = dlg->GetDlgItemInt(IDC_EDIT_CHANNEL);
    format = dlg->m_combo.GetCurSel();
    switch (format)
    {
    case 0:
    case 1:
        sampleBytes = 2; // S16, U16
        break;
    case 2:
    case 3:
        sampleBytes = 4; // S32, float
        break;
    default:
        sampleBytes = 4;
        break;
    }
    bufferSize = frameSize * channels * sampleBytes; // һ֡ pcm ���ݻ�������С����������� * ͨ������ * ÿ������ֵ�ֽ���(float)
    buffer = new unsigned char[bufferSize];

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        AfxMessageBox(_T("Couldn't initialize SDL."));
        return 1;
    }

    //SDL_AudioSpec
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = sampleRate;
    switch (format)
    {
    case 0:
        wanted_spec.format = AUDIO_U16SYS; break;
    case 1:
        wanted_spec.format = AUDIO_S16SYS; break;
    case 2:
        wanted_spec.format = AUDIO_S32SYS; break;
    case 3:
        wanted_spec.format = AUDIO_F32SYS; break;
        break;
    default:
        wanted_spec.format = AUDIO_F32SYS; break;
        break;
    }
    wanted_spec.channels = channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = frameSize;
    wanted_spec.callback = fill_audio;

    if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
        AfxMessageBox(_T("Couldn't open audio."));
        return 1;
    }

    SDL_PauseAudio(0);

    while (!feof(fp)) {
        if (fread(buffer, 1, bufferSize, fp) != bufferSize) {
            break;
        }

        audio_chunk = (Uint8 *)buffer;
        audio_len = bufferSize;
        audio_pos = audio_chunk;

        while (audio_len > 0) // �ȴ��ϴν�������Ƶ������ϣ�Ȼ���ٽ�����һ֡
            SDL_Delay(1);
    }

    delete[] buffer;
    SDL_Quit();

    return 0;
}


void CPcmShowDlg::OnClickedButtonPlay()
{
    // TODO: Add your control notification handler code here
    AfxBeginThread(threadFuncPlay, this);
}

