// C:\code\FFmpeg\ffmpegStudy\ffmpegStudy\encodeYuvToH264Dlg\encodeYuvToH264Dlg.cpp : implementation file
//

#include "../pch.h"
#include "../ffmpegStudy.h"
#include "encodeYuvToH264Dlg.h"
#include "afxdialogex.h"


extern "C"
{
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
};

#define OUTPUT_H264_FILENAME     "temp_output.h264"

// CEncodeYuvToH264Dlg dialog

IMPLEMENT_DYNAMIC(CEncodeYuvToH264Dlg, CDialogEx)

CEncodeYuvToH264Dlg::CEncodeYuvToH264Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_YUVTOH264, pParent)
{

}

CEncodeYuvToH264Dlg::~CEncodeYuvToH264Dlg()
{
}

void CEncodeYuvToH264Dlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MFCEDITBROWSE, m_browse);
}


BEGIN_MESSAGE_MAP(CEncodeYuvToH264Dlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_PLAY, &CEncodeYuvToH264Dlg::OnClickedButtonPlay)
END_MESSAGE_MAP()


// CEncodeYuvToH264Dlg message handlers


void CEncodeYuvToH264Dlg::OnClickedButtonPlay()
{
    // TODO: Add your control notification handler code here
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame;
    AVPacket *packet;
    AVCodecID codec_id = AV_CODEC_ID_H264;

    FILE *fpIn;
    FILE *fpOut;
    int frameWidth, frameHeight, frameSize, frameCnt = 100;
    int i, ret, got_output;

    CString strInFileName;
    char * chInFileName;
    // 输入 .yuv 文件，进行 yuv->h264 https://blog.csdn.net/leixiaohua1020/article/details/42181271
    m_browse.GetWindowTextW(strInFileName);
    if (strInFileName.IsEmpty()) {
        return;
    }
    USES_CONVERSION;
    chInFileName = W2A(strInFileName);

    fpIn = fopen(chInFileName, "rb");
    fpOut = fopen(OUTPUT_H264_FILENAME, "wb");

    frameWidth = GetDlgItemInt(IDC_EDIT_WIDTH);
    frameHeight = GetDlgItemInt(IDC_EDIT_HEIGHT);
    frameSize = frameWidth * frameHeight;

    pCodec = avcodec_find_encoder(codec_id);
    if (!pCodec) {
        printf("Codec not found\n");
        return;
    }

    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) {
        printf("Could not allocate video codec context\n");
        return;
    }

    pCodecCtx->bit_rate = 400000;
    pCodecCtx->width = frameWidth;
    pCodecCtx->height = frameHeight;
    pCodecCtx->time_base.num = 1;
    pCodecCtx->time_base.den = 25;
    pCodecCtx->gop_size = 10;
    pCodecCtx->max_b_frames = 1;
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    av_opt_set(pCodecCtx->priv_data, "preset", "slow", 0);

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec\n");
        return;
    }

    pFrame = av_frame_alloc();
    packet = av_packet_alloc();

    pFrame->format = pCodecCtx->pix_fmt;
    pFrame->width = pCodecCtx->width;
    pFrame->height = pCodecCtx->height;

    // 分配 pFrame->data 内存空间， 16 字节对齐
    ret = av_image_alloc(pFrame->data, pFrame->linesize, pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, 16);
    if (ret < 0) {
        printf("Could not allocate raw picture buffer\n");
        return;
    }

    for (i = 0; i < frameCnt; i++) { // h264 文件中存储 100 帧编码数据
        if (fread(pFrame->data[0], 1, frameSize, fpIn) <= 0 ||      // Y
            fread(pFrame->data[1], 1, frameSize/4, fpIn) <= 0 ||    // U
            fread(pFrame->data[2], 1, frameSize/4, fpIn) <= 0) {    // V
            return;
        }

        pFrame->pts = i;
        ret = avcodec_send_frame(pCodecCtx, pFrame);
        if (ret < 0) {
            printf("Error encoding frame\n");
            return;
        }
        got_output = avcodec_receive_packet(pCodecCtx, packet);
        if (!got_output) {
            fwrite(packet->data, 1, packet->size, fpOut);
            av_packet_unref(packet);
        }
    }

    fclose(fpIn);
    fclose(fpOut);
    avcodec_close(pCodecCtx);
    av_free(pCodecCtx);
    av_freep(&pFrame->data[0]);
    av_frame_free(&pFrame);
    av_packet_free(&packet);

    MessageBox(_T("complete"));
}
