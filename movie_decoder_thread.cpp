#include <QString>
#include <QDebug>
#include <QBuffer>
#include <QImage>
#include <iostream>
#include <stdio.h>
#include "movie_decoder_thread.h"

// compatibility with newer API

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

void movie_decoder_thread::seek_request_slot(int seek_pos)
{
}
movie_decoder_thread::movie_decoder_thread(QObject *parent) : QThread(parent)
{

    fmt_ctx = NULL;
    video_dec_ctx = NULL;
    audio_dec_ctx = NULL; //was not originally null
    video_stream = NULL;
    audio_stream = NULL;
    src_filename = NULL;
    for(int i = i; i<4; i++)
    {
        video_dst_data[i] = NULL;
    }

    video_stream_idx = -1;
    audio_stream_idx = -1;
    frame = NULL;
    video_frame_count = 0;
    audio_frame_count = 0;

    //picture rescaler variables
    rsc_pix_fmt = AV_PIX_FMT_RGB565;
    rsc_size = NULL;

    //audio resampler variables
    dst_ch_layout = AV_CH_LAYOUT_STEREO;
    dst_data = NULL;

    continue_loop = false;
    myQueue = new(safe_queue);
    myQueue->lock.lock();
    myQueue->data = (unsigned char**)malloc(sizeof(unsigned char*)*QUEUE_SIZE);
    myQueue->head = 0;
    myQueue->tail = 0;
    myQueue->queue_size = 0;
    myQueue->lock.unlock();

    myIWM = new (image_with_mutex);
}
movie_decoder_thread::~movie_decoder_thread()
{
    fprintf(stderr,"Initializing the deconstruction of thread. \n");
    //free the sound queue.
    init_queue(myQueue);
    free(myQueue->data);

    delete myIWM;

    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);

    //free picture rescaler stuff if it has been initialized
    if(picture_rescaling_needed)
    {
        av_freep(&rsc_data[0]);
        sws_freeContext(sws_ctx);
    }

    if(audio_resampling_needed)
    {
        if (dst_data) //will it work like this? (i guess yes because it was made null)
        {
            av_freep(&dst_data[0]);
        }
        av_freep(&dst_data);
        swr_free(&swr_ctx);
    }

    fprintf(stderr,"Have reached the end of the deconstruction of thread. \n");

}
void movie_decoder_thread::put_in_queue(safe_queue *queue, unsigned char *frame)
{

    queue->lock.lock();
    if(queue->queue_size >= QUEUE_SIZE)
    {
        printf("Waiting for data to be emptied...\n");
        queue->emptied_cond.wait(&queue->lock);
    }
    queue->data[queue->head] = frame;
    //printf("head is %d.\n",queue->head);
    queue->head++;
    queue->head = queue->head%QUEUE_SIZE;
    queue->queue_size++;
    //printf("%d size of the queue is.\n",queue->queue_size);
    queue->filled_cond.wakeOne();
    queue->lock.unlock();

}
int movie_decoder_thread::init_queue(safe_queue *queue)
{
    //delete all of the data in the audio_queue here

    queue->lock.lock();
    for(int i = 0 ; i < queue->queue_size; i++)
    {
       free(queue->data[(queue->tail + i)%QUEUE_SIZE]);
    }

    queue->head = 0;
    queue->tail = 0;
    queue->queue_size = 0;
    queue->lock.unlock();
}

int movie_decoder_thread::setFilename(QString filename_string)
{
    int ret = 0;

    filename = filename_string;
    qDebug() << "Filename is: " << filename << "\n";
    src_filename = filename.toLatin1().constData();
    fprintf(stderr, "source filename: %s\n", src_filename);

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0)
    {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        goto end;
    }
    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
    {
        fprintf(stderr, "Could not find stream information\n");
        goto end;
    }
    if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0)
    {
        video_stream = fmt_ctx->streams[video_stream_idx];

        /* allocate image where the decoded image will be put */
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        ret = av_image_alloc(video_dst_data, video_dst_linesize,width, height, pix_fmt, 1);

        if (ret < 0)
        {
            fprintf(stderr, "Could not allocate raw video buffer\n");
            goto end;
        }
        video_dst_bufsize = ret;
    }
    if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0)
    {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
    }
    /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, src_filename, 0);

    if (!audio_stream && !video_stream) //maybe can do this stuff beforee
    {
        fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
        goto end; //in original code. here unallocates stuff
    }
    frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

     if(!video_stream && !audio_stream) //maybe over engineering here a bit.
     {
         //nothing to display
         goto end;
     }

    //Picture rescaler initialisation
    //do this if there is a need for conversion.
    /* create scaling context */

    rsc_w  = width;
    rsc_h  = height;

    if(rsc_w!=width || rsc_h!=height || pix_fmt!=rsc_pix_fmt)
    {
        picture_rescaling_needed = true;

        if (!sws_ctx)
        {
            fprintf(stderr,
                    "Impossible to create scale context for the conversion "
                    "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
                    av_get_pix_fmt_name(pix_fmt), width, height,
                    av_get_pix_fmt_name(rsc_pix_fmt), rsc_w, rsc_h);
            ret = AVERROR(EINVAL);
            goto end; //think here how to handle
        }

        /* buffer is going to be written to rawvideo file, no alignment */ //this alignment thing must be looked at.
        if ((ret = av_image_alloc(rsc_data, rsc_linesize, rsc_w, rsc_h, rsc_pix_fmt, 1)) < 0)
        {
            fprintf(stderr, "Could not allocate destination image\n");
            goto end; //think here how to handle
        }
        rsc_bufsize = ret;

        sws_ctx = sws_getContext(width, height, pix_fmt,
                                 rsc_w, rsc_h, rsc_pix_fmt,
                                 SWS_BILINEAR, NULL, NULL, NULL);
    }
    else
    {
        picture_rescaling_needed = false;
    }


    if(audio_dec_ctx->sample_fmt != AV_SAMPLE_FMT_S16)
    {
        audio_resampling_needed = true;
        //we need to have a audio rescaler
        //also check the frequency. (maybe)

        src_ch_layout = audio_dec_ctx->channel_layout;
        src_rate = audio_dec_ctx->sample_rate;
        dst_rate = src_rate; //ensure here that the rate us supported by our playback thread.
        src_nb_channels = 0;
        dst_nb_channels = 0;
        src_nb_samples = audio_dec_ctx->frame_size == 0 ? 1536 : audio_dec_ctx->frame_size;
        src_sample_fmt = audio_dec_ctx->sample_fmt;
        dst_sample_fmt = AV_SAMPLE_FMT_S16;

        fprintf(stderr, "src_nb_samples %d, src_rate: %d \n", src_nb_samples, src_rate);

        /* create resampler context */
        swr_ctx = swr_alloc();
        if (!swr_ctx)
        {
            fprintf(stderr, "Could not allocate resampler context\n");
            ret = AVERROR(ENOMEM);
            goto resampler_fail_end; //this end is wrong.
        }
        /* set options */
        av_opt_set_int(swr_ctx, "in_channel_layout",    src_ch_layout, 0);
        av_opt_set_int(swr_ctx, "in_sample_rate",       src_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);
        av_opt_set_int(swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
        av_opt_set_int(swr_ctx, "out_sample_rate",       dst_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);
        /* initialize the resampling context */
        if ((ret = swr_init(swr_ctx)) < 0)
        {
            fprintf(stderr, "Failed to initialize the resampling context\n");
            goto resampler_fail_end; //this end is wrong
        }

        /* compute the number of converted samples: buffering is avoided
         * ensuring that the output buffer will contain at least all the
         * converted input samples */
        max_dst_nb_samples = dst_nb_samples = av_rescale_rnd(src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
        /* buffer is going to be directly written to a rawaudio file, no alignment */
        dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
        ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,dst_nb_samples, dst_sample_fmt, 0);

        if (ret < 0)
        {
            fprintf(stderr, "Could not allocate destination samples\n");
            goto resampler_fail_end; //this end is wrong
        }
    }
    else
    {
        audio_resampling_needed = false;
    }

    audio_samples_for_queue = max_dst_nb_samples;

    audio_start_context.mutex.lock();
    audio_start_context.queue = myQueue;
    audio_start_context.buffer_size = audio_samples_for_queue<<2; //inbytes; (should get from audio resampler perhaps)
    audio_start_context.frequency =  dst_rate; //should not be fixed.
    audio_start_context.queue_size = QUEUE_SIZE;
    audio_start_context.mutex.unlock();

    //image_buffer = video_dst_data[0];
    image_buffer = (unsigned char*)malloc(rsc_bufsize);
    if(picture_rescaling_needed)
    {
        image_buffer = rsc_data[0];
    }
    else
    {
        image_buffer = video_dst_data[0];
    }
    myIWM->mutex.lock(); //use wait condition
    myIWM->image = new QImage ((unsigned char*)image_buffer , width, height, QImage::Format_RGB16); //can also use 565 to save memory (as using here.)
    myIWM->mutex.unlock(); //use wait condition

    /*
    printf("Play the output video file with the command:\n"
            "ffplay -f rawvideo -pix_fmt %s -video_size %dx%d \n",
            av_get_pix_fmt_name(pix_fmt), width, height
            );
    */

    return 0; //success

resampler_fail_end:

    if (dst_data) //will it work like this? (i guess yes because it was made null)
    {
        av_freep(&dst_data[0]);
    }
    av_freep(&dst_data);
    swr_free(&swr_ctx);
end:
    //do we need to free resamplers and scalers here??

    //if anything fails, free stuff and notify the caller.
    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);

    return -1; //failure


// Do all the initialization here.
}
int movie_decoder_thread::open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0)
    {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",av_get_media_type_string(type), src_filename);
        return ret;
    } else
    {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec)
        {
            fprintf(stderr, "Failed to find %s codec\n",av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx)
        {
            fprintf(stderr, "Failed to allocate the %s codec context\n",av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }
        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0)
        {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",av_get_media_type_string(type));
            return ret;
        }
        /* Init the decoders, with or without reference counting */
        av_dict_set(&opts, "refcounted_frames", 0 ? "1" : "0", 0); //zero means without reference counting.
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0)
        {
            fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }
    return 0;
}

int movie_decoder_thread::decode_packet(int *got_frame, int cached)
{
    int ret = 0;
    int decoded = pkt.size;
    *got_frame = 0;
    if (pkt.stream_index == video_stream_idx)
    {
        /* decode video frame */
        ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0)
        {
            fprintf(stderr, "Error decoding video frame (%s)\n", av_err2str(ret));
            return ret;
        }
        if (*got_frame)
        {
            if (frame->width != width || frame->height != height || frame->format != pix_fmt)
            {
                /* To handle this change, one could call av_image_alloc again and
                 * decode the following frames into another rawvideo file. */
                fprintf(stderr, "Error: Width, height and pixel format have to be "
                        "constant in a rawvideo file, but the width, height or "
                        "pixel format of the input video changed:\n"
                        "old: width = %d, height = %d, format = %s\n"
                        "new: width = %d, height = %d, format = %s\n",
                        width, height, av_get_pix_fmt_name(pix_fmt),
                        frame->width, frame->height,
                        av_get_pix_fmt_name((AVPixelFormat)frame->format));
                return -1;
            }
            printf("video_frame%s n:%d coded_n:%d\n",
                   cached ? "(cached)" : "",
                   video_frame_count++, frame->coded_picture_number);
            /* copy decoded frame to destination buffer:
             * this is required since rawvideo expects non aligned data */
            myIWM->mutex.lock();
            av_image_copy(video_dst_data, video_dst_linesize,(const uint8_t **)(frame->data), frame->linesize,pix_fmt, width, height);

            if(picture_rescaling_needed)
            {
                sws_scale(sws_ctx, (const uint8_t * const*)video_dst_data, video_dst_linesize, 0, height, rsc_data, rsc_linesize);
            }

            //sws_scale(sws_ctx, (const uint8_t * const*)src_data, src_linesize, 0, src_h, dst_data, dst_linesize);

            /* write scaled image to file */
            //fwrite(dst_data[0], 1, dst_bufsize, dst_file);

            myIWM->mutex.unlock();
            if(first_frame_after_reset)
            {
                //set the local clock to be this time;
                video_time.start();
                first_frame_after_reset = false;
            }
            else
            {
                unsigned int frame_pts = 1000.0*(double)frame->pts*av_q2d(video_stream->time_base);
                fprintf(stderr, "frame pts no: %d, frame time base (double) = %0.10f \n", frame->pts, av_q2d(video_stream->time_base));

                unsigned int current_time = video_time.elapsed();
                unsigned int video_ahead_by = (frame_pts - current_time)*1000;
                if(video_ahead_by > 0)
                {
                    usleep(video_ahead_by); //or find something better if this has issues. not perfect frame rate.
                }
                fprintf(stderr, "Current time = %0.3f, pts time = %0.3f \n", (float)current_time/1000, (float)frame_pts/1000);
            }
            emit frameDecoded(myIWM);

            //fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file); //here we will send to image
        }
    } else if (pkt.stream_index == audio_stream_idx)
    {
        /* decode audio frame */
        ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0)
        {
            fprintf(stderr, "Error decoding audio frame (%s)\n", av_err2str(ret));
            return ret;
        }
        /* Some audio decoders decode only part of the packet, and have to be
         * called again with the remainder of the packet data.
         * Sample: fate-suite/lossless-audio/luckynight-partial.shn
         * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret, pkt.size);
        if (*got_frame)
        {
            size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)frame->format);
            printf("audio_frame%s n:%d nb_samples:%d pts:%s\n",
                   cached ? "(cached)" : "",
                   audio_frame_count++, frame->nb_samples,
                   av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));

            /* Write the raw audio data samples of the first plane. This works
             * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
             * most audio decoders output planar audio, which uses a separate
             * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
             * In other words, this code will write only the first audio channel
             * in these cases.
             * You should use libswresample or libavfilter to convert the frame
             * to packed data. */

           // fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file); convert and write add to queue.

            dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, src_rate) + src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);

            if (dst_nb_samples > max_dst_nb_samples) //if this changes our queue will not be able to keep up basically
            {
                av_freep(&dst_data[0]);
                ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels, dst_nb_samples, dst_sample_fmt, 1);

                //reinitalize the queue here.

                if (ret < 0)
                {
                    //stop the main loop
                    //break; do something
                }
                max_dst_nb_samples = dst_nb_samples;
            }
            /* convert to destination format */
            ret = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)frame->extended_data, src_nb_samples);
            if (ret < 0)
            {
                fprintf(stderr, "Error while converting\n");
                //stop the main loop
            }

            dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,ret, dst_sample_fmt, 1);

            if (dst_bufsize < 0)
            {
                fprintf(stderr, "Could not get sample buffer size\n");
                //stop the main loop
            }
           // printf("t:%f in:%d out:%d\n", t, src_nb_samples, ret);
            //fwrite(dst_data[0], 1, dst_bufsize, dst_file);

            audio_data = (unsigned char*)malloc(audio_samples_for_queue<<2); //also will need to change this is queue changes.
            memcpy(audio_data, dst_data[0], audio_samples_for_queue<<2);
            put_in_queue(myQueue, audio_data);
        }
    }
    /* dont need this as we are not using refcount
    if (*got_frame && refcount)
        av_frame_unref(frame);
    */
    return decoded;
}
void movie_decoder_thread::run(){

    int ret, got_frame;
    //send signal to start audio playback thread;
    emit audio_capture_started(&audio_start_context); //mutex requrired??
    qDebug("video capture thread sends signal to video playback thread to start audio playback.");

    first_frame_after_reset = true;
    continue_loop = true;

    //must seek here accordingly.

    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0 && continue_loop)
    {
        AVPacket orig_pkt = pkt;

        do
        {
            ret = decode_packet(&got_frame, 0);
            if (ret < 0)
            {
                break;
            }
            pkt.data += ret;
            pkt.size -= ret;

        }
        while (pkt.size > 0);
        av_packet_unref(&orig_pkt);
    }

    /* flush cached frames */
    pkt.data = NULL;
    pkt.size = 0;
    do
    {
       decode_packet(&got_frame, 1);
    }
    while (got_frame);

    qDebug("video stopped x.");
    emit movie_stopped_signal(); //connects to both audio thread and parent.

}

void movie_decoder_thread::stopThread()
{
    continue_loop = false; // allow the run command finish by ending while //may need mutex
    this->wait();          //wait for the thread to finish
    qDebug("The video decoder thread has successfully finished");
}

void movie_decoder_thread::startThread()
{
    qDebug()<<"The video decoder thread is starting!";
    continue_loop = true;
    this->start();
}
