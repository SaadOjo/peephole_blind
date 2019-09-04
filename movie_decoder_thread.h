#ifndef MOVIE_DECODER_THREAD_H
#define MOVIE_DECODER_THREAD_H

#include <QThread>
#include <QImage>
#include <QTime>
#include <QMutex>
#include <QDebug>
#include <QFileDialog> //temp
#include <sys/time.h> //temp
#include "structures.h"

extern "C"
{

#define __STDC_CONSTANT_MACROS // for UINT64_C
#define __STDC_FORMAT_MACROS


#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

//#include <libavutil/imgutils.h>
//#include <libavutil/parseutils.h>
#include <libswscale/swscale.h>

#include <libavutil/opt.h>  //do we need all of this?
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

}

#define AUDIO_BUFFER_SIZE 1024 //maybe not.
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
    void startThread(); //can define filename to start thread probably.
    int  setFilename(QString filename);
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
    int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type);
    int decode_packet(int *got_frame, int cached);
    QString filename;
    bool continue_loop;

    void put_in_queue(safe_queue *queue, unsigned char *frame);
    int init_queue(safe_queue *queue);

    int total_movie_duration;  //seconds
    seek_request my_seek_request;
    video_state  my_video_state;
    struct timeval timer_start, timer_now;

    //decoder variables
     AVFormatContext      *fmt_ctx;
     AVCodecContext       *video_dec_ctx, *audio_dec_ctx;
     int                  width, height;
     enum AVPixelFormat   pix_fmt;
     AVStream             *video_stream, *audio_stream;
     char                 *src_filename; //changed from const char
     uint8_t              *video_dst_data[4];
     int                  video_dst_linesize[4];
     int                  video_dst_bufsize;
     int                  video_stream_idx, audio_stream_idx;
     AVFrame              *frame;
     AVPacket             pkt;
     int                  video_frame_count;
     int                  audio_frame_count;

     unsigned char*      image_buffer;
     bool                first_frame_after_reset;

     //picture rescaler variables
     bool                picture_rescaling_needed;
     uint8_t             *rsc_data[4];
     int                 rsc_linesize[4];
     int                 rsc_w, rsc_h;
     enum AVPixelFormat  rsc_pix_fmt;
     const char          *rsc_size;
     int                 rsc_bufsize;
     struct SwsContext   *sws_ctx;

     //Timing variables
     QTime              video_time;

     //audio resampler variables
     bool                   audio_resampling_needed;
     int64_t                src_ch_layout, dst_ch_layout;
     int                    src_rate, dst_rate;
     uint8_t                **dst_data;
     int                    src_nb_channels, dst_nb_channels;
     int                    src_linesize, dst_linesize;
     int                    src_nb_samples, dst_nb_samples, max_dst_nb_samples;
     enum AVSampleFormat    src_sample_fmt, dst_sample_fmt;
     int                    dst_bufsize;
     const char             *fmt;
     struct SwrContext      *swr_ctx;

     int                    audio_samples_for_queue;
     unsigned char*         audio_data;
};

#endif // MOVIE_DECODER_THREAD_H
