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
    if(!my_seek_request.req)
    {
        my_seek_request.seek_pos = seek_pos*total_movie_duration/100;
        my_seek_request.seek_flags = (my_seek_request.seek_pos > my_video_state.video_clock) ? 0 : AVSEEK_FLAG_BACKWARD;
        fprintf(stderr, "vc: %0.1f, sp: %0.1f \n",my_video_state.video_clock, my_seek_request.seek_pos);
        //if(fabs(my_seek_request.seek_pos - my_video_state.video_clock) > 1)
        {
            my_seek_request.req = true;
            my_video_state.do_delay = false;

        }

        //display request summary
        //emit movie_stopped_signal();
        //stopThread();
        gettimeofday(&timer_now,NULL);
        int seek_pos_seconds = my_seek_request.seek_pos; //implicit conversion from double to int.
        timer_start.tv_sec  = timer_now.tv_sec - seek_pos_seconds;
        timer_start.tv_usec =  timer_now.tv_usec - 1000000*(my_seek_request.seek_pos - (double)seek_pos_seconds);
        my_video_state.current_time = my_seek_request.seek_pos;
        fprintf(stderr, "seek pos: %0.1f, seek flag: %d, seek_request: %d\n",my_seek_request.seek_pos, my_seek_request.seek_flags, my_seek_request.req);
        //while(1);

    }
}

movie_decoder_thread::movie_decoder_thread(QObject *parent) : QThread(parent)
{

    pFormatCtx = NULL; //Set to NULL to check is we have allocated stuff.
    pCodecCtxOrig = NULL;
    pCodecCtx = NULL;
    pCodec = NULL;
    pFrame = NULL;
    pFrameRGB = NULL;
    buffer = NULL;
    sws_ctx = NULL;

    aCodecCtxOrig = NULL;
    aCodecCtx = NULL;
    aCodec = NULL;

    //audio swr
    //src_ch_layout = AV_CH_LAYOUT_STEREO; //get from stream
    dst_ch_layout = AV_CH_LAYOUT_STEREO;
    //src_sample_fmt = AV_SAMPLE_FMT_S32P; //get from data.
    dst_sample_fmt = AV_SAMPLE_FMT_S16;
    //src_rate = 44100; //get from stream.
    dst_rate = 44100;
    //src_nb_samples = 1024; //get from stream
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

    my_seek_request.req = false;
    my_seek_request.seek_pos = 0;

    my_video_state.video_clock = 0;
    my_video_state.do_delay = true;
}
movie_decoder_thread::~movie_decoder_thread()
{
    fprintf(stderr,"Initializing the deconstruction of thread. \n");
    myQueue->lock.lock();

    //free the sound queue.
    for(int i = 0 ; i < myQueue->queue_size; i++)
    {
        free(myQueue->data[(myQueue->tail + i)%QUEUE_SIZE]);
    }
    myQueue->lock.unlock();

    free(myQueue->data);

    // Free the RGB image
    av_free(buffer);
    av_frame_free(&pFrameRGB);

    // Free the YUV frame
    av_frame_free(&pFrame);
    av_frame_free(&frame);

    // Close the codecs
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);

    //Close the audio codecs
    avcodec_close(aCodecCtx);
    avcodec_close(aCodecCtxOrig);

    // Close the video file
    avformat_close_input(&pFormatCtx);

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

void movie_decoder_thread::init_audio_swr()
{
    swr_ctx = swr_alloc();
    if (!swr_ctx)
    {
        fprintf(stderr, "Could not allocate resampler context\n");
        ret = AVERROR(ENOMEM);
        //free swctx.
    }
    /* set options */
    av_opt_set_int(swr_ctx, "in_channel_layout",    src_ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate",       src_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);
    av_opt_set_int(swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate",       dst_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);
    /* initialize the resampling context */
    if ((ret = swr_init(swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        //free swctx.
    }

    /* compute the number of converted samples: buffering is avoided
     * ensuring that the output buffer will contain at least all the
     * converted input samples */
    max_dst_nb_samples = dst_nb_samples =
        av_rescale_rnd(src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
    /* buffer is going to be directly written to a rawaudio file, no alignment */
    dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
    ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
                                             dst_nb_samples, dst_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate destination samples\n");
        //free data and swrctx
    }
}

int movie_decoder_thread::decode_audio_packet(int *got_frame, int cached)
{
    int ret = 0;
    int decoded = packet.size;
    *got_frame = 0;
    int dst_bufsize;

        /* decode audio frame */
        ret = avcodec_decode_audio4(aCodecCtx, frame, got_frame, &packet);
        if (ret < 0) {
            fprintf(stderr, "Error decoding audio frame (%s)\n", av_err2str(ret));
            return ret;
        }
        /* Some audio decoders decode only part of the packet, and have to be
         * called again with the remainder of the packet data.
         * Sample: fate-suite/lossless-audio/luckynight-partial.shn
         * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret, packet.size);
        if (*got_frame) {
            size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)frame->format);
            //printf("audio_frame size %d\n",unpadded_linesize);

                     /*
            printf("audio_frame%s n:%d nb_samples:%d pts:%s\n",
                   cached ? "(cached)" : "",
                   audio_frame_count++, frame->nb_samples,
                   av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));
                   */
            /* Write the raw audio data samples of the first plane. This works
             * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
             * most audio decoders output planar audio, which uses a separate
             * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
             * In other words, this code will write only the first audio channel
             * in these cases.
             * You should use libswresample or libavfilter to convert the frame
             * to packed data. */

            //frame->extended_data[0], 1, unpadded_linesize;
            //second channel indata2

            //unsigned char sound[1024*2*2];
            /* compute destination number of samples */

            dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, src_rate) +
                                            src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
            if (dst_nb_samples > max_dst_nb_samples) {
                av_freep(&dst_data[0]);
                ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,
                                       dst_nb_samples, dst_sample_fmt, 1);
                if (ret < 0)
                    exit(-1);//break;
                max_dst_nb_samples = dst_nb_samples;
            }

            // convert to destination format
            ret = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)frame->extended_data, src_nb_samples);
            if (ret < 0) {
                fprintf(stderr, "Error while converting\n");

                //free stuff
            }
            dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
                                                     ret, dst_sample_fmt, 1);
            if (dst_bufsize < 0) {
                fprintf(stderr, "Could not get sample buffer size\n");
                //free stuff
            }
            //printf("t:%f in:%d out:%d\n", t, src_nb_samples, ret);
            //fwrite(dst_data[0], 1, dst_bufsize, dst_file);

            qDebug()<< "Size of dst_buff" << dst_bufsize;
            unsigned char *sound = (unsigned char*)malloc(dst_bufsize);
            //memcpy(sound,dst_data[0],dst_bufsize);
            memcpy(sound,dst_data[0],dst_bufsize);
            put_in_queue(myQueue,sound);

        }

        printf("successfully decoded stuff..\n");

    return decoded;
}

void movie_decoder_thread::setFilename(QString filename_string)
{
    int x_dst_bufsize;
    //NOTE: Cannot easily change order of the code.
    //It depends a lot the way things are ended. (freed.)
    filename = filename_string;
    qDebug() << "Filename is: " << filename << "\n";

    // Open video file
    if(avformat_open_input(&pFormatCtx, filename.toLatin1().constData(), NULL, NULL)!=0) //gives error 11 when does not find file
    {
        goto end;
    }
    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
    {
        goto close_fmt_end;
    }

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, filename.toLatin1().constData(), 0);
    total_movie_duration =  pFormatCtx->duration/AV_TIME_BASE;
    //fprintf(stderr," duration %d, \n",total_movie_duration);
    emit set_time_left(total_movie_duration);

    // Find the first video stream
    videoStream=-1;
    audioStream=-1;

    for(i=0; i<pFormatCtx->nb_streams; i++) //find and label streams
    {
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
        {
            videoStream=i; //cannot have breaks because of other stream.
        }

        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO && audioStream < 0)

        {
            audioStream=i;
        }
    }

    if(videoStream==-1)
    {
        goto close_fmt_end; // Couldn't find video stream
    }
    if(audioStream==-1)
    {
        goto close_fmt_end; // Couldn't find audio stream
    }

    //AUDIO CODEC
    aCodecCtxOrig=pFormatCtx->streams[audioStream]->codec;
    aCodec = avcodec_find_decoder(aCodecCtxOrig->codec_id);
    if(!aCodec)
    {
        fprintf(stderr, "Unsupported codec!\n");
        goto close_acodecs_end;
    }

    // Copy context
    aCodecCtx = avcodec_alloc_context3(aCodec);
    if(avcodec_copy_context(aCodecCtx, aCodecCtxOrig) != 0)
    {
      fprintf(stderr, "Couldn't copy codec context");
      goto close_acodecs_end;
    }

    // Open codec
    if(avcodec_open2(aCodecCtx, aCodec, NULL)<0)
    {
        fprintf(stderr, "Couldn't open audio codec! \n");
        goto close_acodecs_end;

    }

    frame = av_frame_alloc(); //do we need to do this? (YES)
    if (!frame)
    {
        fprintf(stderr, "Could not allocate frame.\n");
        goto free_frame_end;
    }


    //set data about stream here for the audio resamlpler

    src_ch_layout = aCodecCtx->channel_layout;
    src_rate = aCodecCtx->sample_rate;
    src_rate = 8000;
    src_sample_fmt = aCodecCtx->sample_fmt;
    src_nb_samples = aCodecCtx->frame_size;

    if(src_nb_samples == 0)
    {
        src_nb_samples = 1536;
    }

    init_queue(myQueue);
    init_audio_swr();

    x_dst_bufsize = max_dst_nb_samples *4;
    qDebug("sending dest buff size to audio player %d \n",x_dst_bufsize);

    audio_start_context.mutex.lock();
    audio_start_context.queue = myQueue;
    audio_start_context.buffer_size = x_dst_bufsize;
    audio_start_context.frequency =  aCodecCtx->sample_rate; //should not be fixed.
    audio_start_context.queue_size = QUEUE_SIZE;
    //add buff size here
    audio_start_context.mutex.unlock();

    //VIDEO CODEC
    // Get a pointer to the codec context for the video stream
    pCodecCtxOrig=pFormatCtx->streams[videoStream]->codec;
    // Find the decoder for the video stream
    pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec==NULL)
    {
        fprintf(stderr, "Unsupported codec!\n");
        goto close_pcodecs_end;
    }

    // Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
      fprintf(stderr, "Couldn't copy codec context. \n");
      goto close_pcodecs_end;
    }

    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
    {
        fprintf(stderr, "Couldn't open pcodec. \n");
        goto close_pcodecs_end;
    }

    // Allocate video frame
    pFrame=av_frame_alloc();
    if(pFrame==NULL)
    {
        fprintf(stderr, "Couldn't allocate pFrame. \n");
        goto free_pFrame_end;
    }

    // Allocate an AVFrame structure
    pFrameRGB=av_frame_alloc();

    if(pFrameRGB==NULL)
    {
        fprintf(stderr, "Couldn't allocate pFrameRGB. \n");
        goto free_pFrameRGB_end;
    }

    numBytes=avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
    buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

    myIWM->mutex.lock(); //use wait condition
    myIWM->image = new QImage ((unsigned char*) buffer, pCodecCtx->width, pCodecCtx->height, QImage::Format_RGB888);  ;
    myIWM->mutex.unlock(); //use wait condition

    //char header [50];
    //sprintf(header,"P6\n%d %d 255\n",pCodecCtx->width,pCodecCtx->height);

    fprintf(stderr, "numBytes %d, intosizeof %d", numBytes, numBytes*sizeof(uint8_t)); //same as expected
    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,pCodecCtx->width, pCodecCtx->height);

    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width,
                 pCodecCtx->height,
                 pCodecCtx->pix_fmt,
                 pCodecCtx->width,
                 pCodecCtx->height,
                 AV_PIX_FMT_RGB24,
                 SWS_BILINEAR,
                 NULL,
                 NULL,
                 NULL
                 );

//when you reach here normally without any errors. do not free/ close stuff.
goto end;

free_pFrameRGB_end:
    av_frame_free(&pFrameRGB);

free_pFrame_end:
    av_frame_free(&pFrame);
close_pcodecs_end:
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);

free_frame_end:
    av_frame_free(&frame);

close_acodecs_end:
    avcodec_close(aCodecCtx);
    avcodec_close(aCodecCtxOrig);

close_fmt_end:
    avformat_close_input(&pFormatCtx);   //untested

end:
    printf("ended.");


// Do all the initialization here.
}


void movie_decoder_thread::run(){


    int got_frame,ret;
    int frame_number = 0;

    //seek to the start.
    AVRational r_d;
    r_d.num = 1;
    r_d.den = 10;
    int64_t seek_target = av_rescale_q(0, r_d, pFormatCtx->streams[videoStream]->time_base);
    av_seek_frame(pFormatCtx, videoStream, seek_target, my_seek_request.seek_flags);


    //send signal to start audio playback thread;
    emit audio_capture_started(&audio_start_context); //mutex requrired??
    qDebug("video capture thread sends signal to video playback thread to start audio playback.");

    continue_loop = true;
    gettimeofday(&timer_start, NULL);

    while(av_read_frame(pFormatCtx, &packet)>=0 && continue_loop)
    {
        frame_number++;
        if(frame_number%10)
        {
            emit set_slider_value(my_video_state.current_time*100/total_movie_duration);
            emit set_time_left(my_video_state.current_time);
            frame_number = 0;
        }

        //simply add delay or a timer.

    // Is this a packet from the video stream?
    if(packet.stream_index==videoStream)
        {
        myIWM->mutex.lock();

        // Decode video frame
        avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

        // Did we get a video frame?
        if(frameFinished)
        {
            // Convert the image from its native format to RGB
            sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
            pFrame->linesize, 0, pCodecCtx->height,
            pFrameRGB->data, pFrameRGB->linesize);


            //seek code
            if(my_seek_request.req)
            {
                //seek
                AVRational r_d;
                r_d.num = 1;
                r_d.den = 10;
                int64_t seek_target;
                seek_target = my_seek_request.seek_pos*10;
                seek_target = av_rescale_q( seek_target, r_d, pFormatCtx->streams[videoStream]->time_base);
                av_seek_frame(pFormatCtx, videoStream, seek_target, my_seek_request.seek_flags);

                qDebug("Recieved seek command.");
                //flush the buffers
                avcodec_flush_buffers(aCodecCtx);
                avcodec_flush_buffers(pCodecCtx);
                init_queue(myQueue);

                //empty the record queue.
                my_seek_request.req = false;
            }

            double pts;

            if(packet.dts != AV_NOPTS_VALUE) {
              pts = av_frame_get_best_effort_timestamp(pFrame);
            } else {
              pts = 0;
            }
            pts *= av_q2d(pFormatCtx->streams[videoStream]->time_base);

            double frame_delay;

            if(pts != 0) {
              /* if we have pts, set video clock to it */
              my_video_state.video_clock = pts;
            } else {
              /* if we aren't given a pts, set it to the clock */
              pts = my_video_state.video_clock;
            }
            /* update the video clock */
            frame_delay = av_q2d(pFormatCtx->streams[videoStream]->codec->time_base);
            /* if we are repeating a frame, adjust clock accordingly */
            frame_delay += pFrame->repeat_pict * (frame_delay * 0.5);
            my_video_state.video_clock += frame_delay;

            gettimeofday(&timer_now, NULL);
            my_video_state.current_time = timer_now.tv_sec - timer_start.tv_sec + (timer_now.tv_usec - timer_start.tv_usec)/1000000.0;

            if(my_video_state.do_delay)
            {
                usleep((my_video_state.video_clock - my_video_state.current_time) * 100000); //sync refresh
            }
            else
            {
                if(fabs(my_seek_request.seek_pos - my_video_state.video_clock) < 0.2)
                {
                    my_video_state.do_delay = true;
                    //also need a timer.
                    //does not ensure that it returns to true.
                }
            }

            fprintf(stderr, "frame_time: %0.2f, now: %0.2f \n", pts, my_video_state.current_time);


            myIWM->mutex.unlock();
            //end of critical section.
            emit frameDecoded(myIWM);


            }
            // Free the packet that was allocated by av_read_frame
            av_free_packet(&packet);
        }
        else if(packet.stream_index==audioStream)
        {

            do
            {
                ret = decode_audio_packet(&got_frame, 0);
                if (ret < 0)
                    break;
                packet.data += ret;
                packet.size -= ret;
            } while (packet.size > 0);

            // flush cached frames look at this part
            /* causes issues.
            packet.data = NULL;
            packet.size = 0;
            do
            {
                decode_audio_packet(&got_frame, 1);
            } while (got_frame);
            */

        }
        else
        {
            av_free_packet(&packet);
        }

    }

    qDebug("video stopped x.");
    emit movie_stopped_signal();

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
