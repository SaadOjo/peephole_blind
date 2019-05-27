#ifndef MOVIE_ENCODER_THREAD_H
#define MOVIE_ENCODER_THREAD_H

#include <QThread>
#include <QMutex>
#include <QDebug>

#include "structures.h"
#include "program_state.h"


#define STREAM_FRAME_RATE 10 /* 9 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */
//#define STREAM_PIX_FMT    AV_PIX_FMT_RGB565BE /* default pix_fmt */


#define STREAM_DURATION   2.0
#define SCALE_FLAGS SWS_BICUBIC

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

extern "C"
{
#define __STDC_FORMAT_MACROS
#define __STDC_CONSTANT_MACROS // for UINT64_C
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}


typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;
    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;
    AVFrame *frame;
    AVFrame *tmp_frame;
    float t, tincr, tincr2;
    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;



class movie_encoder_thread : public QThread
{
    Q_OBJECT

public:
    movie_encoder_thread(QObject *parent = 0, safe_encode_video_context * vcontext = NULL, safe_encode_audio_context * acontext = NULL, program_state * pstate = NULL);
    ~movie_encoder_thread();

    void stopThread();
    void startThread();

protected:
    void run();
signals:

private slots:
    //void act_on_encoder_video_set_context(safe_encode_video_context* context); //take the queue pointer
    //void act_on_encoder_audio_set_context(safe_encode_audio_context* context);

private:
    program_state* my_program_state;
    bool continue_loop;
    bool got_video_ctx, got_audio_ctx;
    safe_encode_video_context* video_ctx;
    safe_encode_audio_context* audio_ctx;
    safe_queue* my_sound_queue;
    unsigned char* sound_encode_buffer;
    int recording_frequency;
    int nb_samples;
    int filenumber;
    char filename[50];

    void prepare_new_video_file(char * fname);
    bool encode_video_frame();
    bool encode_audio_frame();
    bool cleanup();

    void close_stream(AVFormatContext *oc, OutputStream *ost);
    void add_stream(OutputStream *ost, AVFormatContext *oc,AVCodec **codec,enum AVCodecID codec_id); //contains parameters about audio and video streams
    void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg);
    void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg);
    AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height);
    AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout,int sample_rate, int nb_samples);
    int write_audio_frame(AVFormatContext *oc, OutputStream *ost, unsigned char *data);
    int write_video_frame(AVFormatContext *oc, OutputStream *ost, unsigned char *data);
    void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt);
    int  write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt);

    AVFrame *get_audio_frame(OutputStream *ost,unsigned char* data);
    AVFrame *get_video_frame(OutputStream *ost, unsigned char* data);

    bool get_from_queue(safe_queue *queue, unsigned char *frame, int buffer_size);

    int queue_size;

    int initialized;
    OutputStream video_st, audio_st;
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVCodec *audio_codec, *video_codec;
    int ret;
    int have_video, have_audio;
    int encode_video, encode_audio;
    AVDictionary *opt;
    int i;
};


#endif // MOVIE_ENCODER_THREAD_H
