#ifndef MOVIE_DECODER_THREAD_H
#define MOVIE_DECODER_THREAD_H

#include <QThread>
#include <QImage>
#include <QTime>
#include <QMutex>
#include <QFileDialog> //temp
#include <sys/time.h> //temp
#include "structures.h"

extern "C"
{

#define __STDC_CONSTANT_MACROS // for UINT64_C

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

}

#define AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000
#define QUEUE_SIZE 300

typedef struct seek_request
{
    int req;
    int seek_flags;
    double seek_pos;

}seek_request;

typedef struct video_state
{
    double video_clock;
    double current_time;
    int do_delay;
} video_state;

class movie_decoder_thread : public QThread
{
    Q_OBJECT

public:
    movie_decoder_thread(QObject *parent = 0);
    ~movie_decoder_thread();

    void stopThread();
    void startThread();
    void setFilename(QString filename);
    image_with_mutex *myIWM;
    safe_queue *myQueue;
    start_context audio_start_context;


protected:
    void run();
signals:

    void frameDecoded(image_with_mutex *imageMutex);
    void audio_capture_started(start_context*);
    void movie_stopped_signal();
    void set_time_left(int);
    void set_slider_value(int);

public slots:
    void seek_request_slot(int);

private:

    QString filename;
    bool continue_loop;

    AVFormatContext   *pFormatCtx;
    int               i, videoStream,audioStream;
    AVCodecContext    *pCodecCtxOrig;
    AVCodecContext    *pCodecCtx;
    AVCodec           *pCodec;
    AVFrame           *pFrame,*frame;
    AVFrame           *pFrameRGB;
    AVPacket          packet;
    int               frameFinished;
    int               numBytes;
    uint8_t           *buffer;
    AVCodecContext    *aCodecCtxOrig;
    AVCodecContext    *aCodecCtx;
    AVCodec           *aCodec;
    struct SwsContext *sws_ctx;

    //resampler stuff
    struct SwrContext *swr_ctx;
    int dst_nb_samples;
    int max_dst_nb_samples;
    int dst_nb_channels;
    int dst_linesize;
    uint8_t **dst_data;


    int64_t src_ch_layout; //get from stream
    int64_t dst_ch_layout;

    enum AVSampleFormat src_sample_fmt; //get from data.
    enum AVSampleFormat dst_sample_fmt;

    int ret;
    int src_rate; //get from stream.
    int dst_rate;
    int src_nb_samples; //get from stream

    void init_audio_swr();

    int decode_audio_packet(int *got_frame, int cached);
    void put_in_queue(safe_queue *queue, unsigned char *frame);
    int init_queue(safe_queue *queue);

    int total_movie_duration;  //seconds

    seek_request my_seek_request;
    video_state  my_video_state;
    struct timeval timer_start, timer_now;
};

#endif // MOVIE_DECODER_THREAD_H
