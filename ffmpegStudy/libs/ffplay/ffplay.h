/*
 * Copyright (c) 2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * simple media player based on the FFmpeg libraries
 */

#include <afxcontrolbars.h>
#include <windows.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <windef.h>


#ifdef __cplusplus
extern "C" {
#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavutil/bprint.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"

#if CONFIG_AVFILTER
# include "libavfilter/avfilter.h"
# include "libavfilter/buffersink.h"
# include "libavfilter/buffersrc.h"
#endif
}
#endif
#include "../../ffplayDlg/ffplayDlg.h"

#include <SDL.h>
#include <SDL_thread.h>

#include <assert.h>


#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 25
#define EXTERNAL_CLOCK_MIN_FRAMES 2
#define EXTERNAL_CLOCK_MAX_FRAMES 10

/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

/* Step size for volume control in dB */
#define SDL_VOLUME_STEP (0.75)

/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.04
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* external clock speed adjustment constants for realtime sources based on buffer fullness */
#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

/* polls for possible required screen refresh at least this often, should be less than 1/fps */
#define REFRESH_RATE 0.01

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
/* TODO: We assume that a decoded and resampled frame fits into this buffer */
#define SAMPLE_ARRAY_SIZE (8 * 65536)

#define CURSOR_HIDE_DELAY 1000000

#define USE_ONEPASS_SUBTITLE_RENDER 1

static unsigned sws_flags = SWS_BICUBIC;

typedef struct MyAVPacketList {
    AVPacket pkt;
    struct MyAVPacketList *next;
    int serial;
} MyAVPacketList;

typedef struct PacketQueue {
    MyAVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int64_t duration;
    int abort_request;
    int serial;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

typedef struct AudioParams {
    int freq;
    int channels;
    int64_t channel_layout;
    enum AVSampleFormat fmt;
    int frame_size;
    int bytes_per_sec;
} AudioParams;

typedef struct Clock {
    double pts;           /* clock base */
    double pts_drift;     /* clock base minus time at which we updated the clock */
    double last_updated;
    double speed;
    int serial;           /* clock is based on a packet with this serial */
    int paused;
    int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */
} Clock;

/* Common struct for handling all types of decoded data and allocated render buffers. */
typedef struct Frame {
    AVFrame *frame;
    AVSubtitle sub;
    int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int64_t pos;          /* byte position of the frame in the input file */
    int width;
    int height;
    int format;
    AVRational sar;
    int uploaded;
    int flip_v;
} Frame;

typedef struct FrameQueue {
    Frame queue[FRAME_QUEUE_SIZE];
    int rindex;
    int windex;
    int size;
    int max_size;
    int keep_last;
    int rindex_shown;
    SDL_mutex *mutex;
    SDL_cond *cond;
    PacketQueue *pktq;
} FrameQueue;

enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

enum ShowMode {
    SHOW_MODE_NONE = -1, SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT, SHOW_MODE_NB
};

typedef struct Decoder {
    AVPacket pkt;
    PacketQueue *queue;
    AVCodecContext *avctx;
    int pkt_serial;
    int finished;
    int packet_pending;
    SDL_cond *empty_queue_cond;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
    SDL_Thread *decoder_tid;
} Decoder;

typedef struct VideoState {
    SDL_Thread *read_tid;
    AVInputFormat *iformat;
    int abort_request;
    int force_refresh;
    int paused;
    int last_paused;
    int queue_attachments_req;
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
    int read_pause_return;
    AVFormatContext *ic;
    int realtime;

    Clock audclk;
    Clock vidclk;
    Clock extclk;

    FrameQueue pictq;
    FrameQueue subpq;
    FrameQueue sampq;

    Decoder auddec;
    Decoder viddec;
    Decoder subdec;

    int audio_stream;

    int av_sync_type;

    double audio_clock;
    int audio_clock_serial;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    PacketQueue audioq;
    int audio_hw_buf_size;
    uint8_t *audio_buf;
    uint8_t *audio_buf1;
    unsigned int audio_buf_size; /* in bytes */
    unsigned int audio_buf1_size;
    int audio_buf_index; /* in bytes */
    int audio_write_buf_size;
    int audio_volume;
    int muted;
    struct AudioParams audio_src;
#if CONFIG_AVFILTER
    struct AudioParams audio_filter_src;
#endif
    struct AudioParams audio_tgt;
    struct SwrContext *swr_ctx;
    int frame_drops_early;
    int frame_drops_late;

    enum ShowMode show_mode;
    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
    RDFTContext *rdft;
    int rdft_bits;
    FFTSample *rdft_data;
    int xpos;
    double last_vis_time;
    SDL_Texture *vis_texture;
    SDL_Texture *sub_texture;
    SDL_Texture *vid_texture;

    int subtitle_stream;
    AVStream *subtitle_st;
    PacketQueue subtitleq;

    double frame_timer;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int video_stream;
    AVStream *video_st;
    PacketQueue videoq;
    double max_frame_duration;      // maximum duration of a frame - above this, we consider the jump a timestamp discontinuity
    struct SwsContext *img_convert_ctx;
    struct SwsContext *sub_convert_ctx;
    int eof;

    char *filename;
    int width, height, xleft, ytop;
    int step;

#if CONFIG_AVFILTER
    int vfilter_idx;
    AVFilterContext *in_video_filter;   // the first filter in the video chain
    AVFilterContext *out_video_filter;  // the last filter in the video chain
    AVFilterContext *in_audio_filter;   // the first filter in the audio chain
    AVFilterContext *out_audio_filter;  // the last filter in the audio chain
    AVFilterGraph *agraph;              // audio filter graph
#endif

    int last_video_stream, last_audio_stream, last_subtitle_stream;

    SDL_cond *continue_read_thread;
} VideoState;

#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)

#if CONFIG_AVFILTER
static int opt_add_vfilter(void *optctx, const char *opt, const char *arg);
#endif

void uninit_opts(void);

int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec);

AVDictionary *filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id,
    AVFormatContext *s, AVStream *st, AVCodec *codec);

AVDictionary **setup_find_stream_info_opts(AVFormatContext *s,
    AVDictionary *codec_opts);

static inline
int cmp_audio_fmts(enum AVSampleFormat fmt1, int64_t channel_count1,
                   enum AVSampleFormat fmt2, int64_t channel_count2);

static inline
int64_t get_valid_channel_layout(int64_t channel_layout, int channels);

static int packet_queue_put_private(PacketQueue *q, AVPacket *pkt);

static int packet_queue_put(PacketQueue *q, AVPacket *pkt);

static int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);

/* packet queue handling */
static int packet_queue_init(PacketQueue *q);

static void packet_queue_flush(PacketQueue *q);

static void packet_queue_destroy(PacketQueue *q);

static void packet_queue_abort(PacketQueue *q);

static void packet_queue_start(PacketQueue *q);

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);

static void decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond);

static int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub);

static void decoder_destroy(Decoder *d);

static void frame_queue_unref_item(Frame *vp);

static int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last);

static void frame_queue_destory(FrameQueue *f);

static void frame_queue_signal(FrameQueue *f);

static Frame *frame_queue_peek(FrameQueue *f);

static Frame *frame_queue_peek_next(FrameQueue *f);

static Frame *frame_queue_peek_last(FrameQueue *f);

static Frame *frame_queue_peek_writable(FrameQueue *f);

static Frame *frame_queue_peek_readable(FrameQueue *f);

static void frame_queue_push(FrameQueue *f);

static void frame_queue_next(FrameQueue *f);

/* return the number of undisplayed frames in the queue */
static int frame_queue_nb_remaining(FrameQueue *f);

/* return last shown position */
static int64_t frame_queue_last_pos(FrameQueue *f);

static void decoder_abort(Decoder *d, FrameQueue *fq);

static inline void fill_rectangle(int x, int y, int w, int h);

static int realloc_texture(SDL_Texture **texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture);

static void calculate_display_rect(SDL_Rect *rect,
    int scr_xleft, int scr_ytop, int scr_width, int scr_height,
    int pic_width, int pic_height, AVRational pic_sar);

static void get_sdl_pix_fmt_and_blendmode(int format, Uint32 *sdl_pix_fmt, SDL_BlendMode *sdl_blendmode);

static int upload_texture(SDL_Texture **tex, AVFrame *frame, struct SwsContext **img_convert_ctx);

static void set_sdl_yuv_conversion_mode(AVFrame *frame);

static void video_image_display(VideoState *is);

static inline int compute_mod(int a, int b);

static void video_audio_display(VideoState *s);

static void stream_component_close(VideoState *is, int stream_index);

static void stream_close(VideoState *is);

static void do_exit(VideoState *is);

static void sigterm_handler(int sig);

static void set_default_window_size(int width, int height, AVRational sar);

static int video_open(VideoState *is);

/* display the current picture, if any */
static void video_display(VideoState *is);

static double get_clock(Clock *c);

static void set_clock_at(Clock *c, double pts, int serial, double time);

static void set_clock(Clock *c, double pts, int serial);

static void set_clock_speed(Clock *c, double speed);

static void init_clock(Clock *c, int *queue_serial);

static void sync_clock_to_slave(Clock *c, Clock *slave);

static int get_master_sync_type(VideoState *is);

/* get the current master clock value */
static double get_master_clock(VideoState *is);

static void check_external_clock_speed(VideoState *is);

/* seek in the stream */
static void stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes);

/* pause or resume the video */
static void stream_toggle_pause(VideoState *is);

static void toggle_pause(VideoState *is);

static void toggle_mute(VideoState *is);

static void update_volume(VideoState *is, int sign, double step);

static void step_to_next_frame(VideoState *is);

static double compute_target_delay(double delay, VideoState *is);

static double vp_duration(VideoState *is, Frame *vp, Frame *nextvp);

static void update_video_pts(VideoState *is, double pts, int64_t pos, int serial);

/* called to display each frame */
static void video_refresh(void *opaque, double *remaining_time);

static int queue_picture(VideoState *is, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);

static int get_video_frame(VideoState *is, AVFrame *frame);

#if CONFIG_AVFILTER
static int configure_filtergraph(AVFilterGraph *graph, const char *filtergraph,
    AVFilterContext *source_ctx, AVFilterContext *sink_ctx);

static int configure_video_filters(AVFilterGraph *graph, VideoState *is, const char *vfilters, AVFrame *frame);

static int configure_audio_filters(VideoState *is, const char *afilters, int force_output_format);
#endif  /* CONFIG_AVFILTER */

static int audio_thread(void *arg);

static int decoder_start(Decoder *d, int(*fn)(void *), const char *thread_name, void* arg);

static int video_thread(void *arg);

static int subtitle_thread(void *arg);

/* copy samples for viewing in editor window */
static void update_sample_display(VideoState *is, short *samples, int samples_size);

/* return the wanted number of samples to get better sync if sync_type is video
 * or external master clock */
static int synchronize_audio(VideoState *is, int nb_samples);

/**
 * Decode one audio frame and return its uncompressed size.
 *
 * The processed audio frame is decoded, converted if required, and
 * stored in is->audio_buf, with size in bytes given by the return
 * value.
 */
static int audio_decode_frame(VideoState *is);

/* prepare a new audio buffer */
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len);

static int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params);

/* open a given stream. Return 0 if OK */
static int stream_component_open(VideoState *is, int stream_index);

static int decode_interrupt_cb(void *ctx);

static int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue);

static int is_realtime(AVFormatContext *s);

/* this thread gets the stream from the disk or the network */
static int read_thread(void *arg);

static VideoState *stream_open(const char *filename, AVInputFormat *iformat);

static void stream_cycle_channel(VideoState *is, int codec_type);

static void toggle_full_screen(VideoState *is);

static void toggle_audio_display(VideoState *is);

static void refresh_loop_wait_event(VideoState *is, SDL_Event *event);

static void seek_chapter(VideoState *is, int incr);

/* handle an event sent by the GUI */
static void event_loop(VideoState *cur_stream);

static int opt_format(void *optctx, const char *opt, const char *arg);

static int opt_sync(void *optctx, const char *opt, const char *arg);

static int opt_codec(void *optctx, const char *opt, const char *arg);

UINT ffplay_start(LPVOID lpParam);

