#include "movie_encoder_thread.h"

movie_encoder_thread::movie_encoder_thread(QObject *parent, safe_encode_video_context * vcontext, safe_encode_audio_context *acontext, program_state *pstate) : QThread(parent)
{
    continue_loop = false;
    got_video_ctx = false;
    got_audio_ctx = false;
    filenumber = 0; //is a global parameter (video_state_parameter)
    initialized = false;

    if(pstate != NULL)
    {
        pstate->mutex.lock();
        my_program_state = pstate;
        pstate->mutex.unlock();
    }

    if(vcontext != NULL)
    {
        vcontext->mutex.lock();
        video_ctx = vcontext;
        vcontext->mutex.unlock();
        got_video_ctx = true;
    }

    if(acontext != NULL)
    {
        acontext->mutex.lock();
        audio_ctx = acontext;
        acontext->mutex.unlock();
        got_audio_ctx = true;
    }
}

movie_encoder_thread::~movie_encoder_thread()
{

}

void movie_encoder_thread::run(){


    while(my_sound_queue == NULL) //this has to be before prepare new file for recording frequency.
    {
        audio_ctx->mutex.lock();
        my_sound_queue = audio_ctx->audio_safe_queue;
        qDebug()<<"Waiting for sound information! \n";
        audio_ctx->mutex.unlock();

    }
    audio_ctx->mutex.lock();
    queue_size = audio_ctx->queue_size;
    nb_samples = audio_ctx->nb_samples;
    recording_frequency = audio_ctx->recording_frequency;
    audio_ctx->mutex.unlock();

    char filename_prefix[40];
    my_program_state->mutex.lock();
    strcpy(filename_prefix,my_program_state->settings_state.movie_recording_directory);
    strcat(filename_prefix,my_program_state->settings_state.movie_recording_name_prefix);
    my_program_state->mutex.unlock();


    strcat(filename_prefix,"_%d.mp4");
    sprintf(filename,filename_prefix ,filenumber++);
    prepare_new_video_file(filename);


    qDebug("Queue Size: %d, nb_samples: %d \n", queue_size, nb_samples);
    sound_encode_buffer = (unsigned char*)malloc(nb_samples); //check if freed.

    //while(1){}

    while(!got_video_ctx||!got_audio_ctx) //can change with wait condition
    //while(!got_video_ctx) //can change with wait condition
    {
        qDebug()<<"Waiting for context!";
    }

    while(continue_loop)
    {

        //encode_video_frame();
        //encode_audio_frame();

        //have to check here for timing.
        if(av_compare_ts(video_st.next_pts, video_st.enc->time_base,audio_st.next_pts, audio_st.enc->time_base) <= 0)
        {
            encode_video_frame();
        } else
        {
            encode_audio_frame();
        }

    }

    cleanup();

}

bool movie_encoder_thread::encode_video_frame()
{
    bool ret = false;
    if(initialized)
    {
        video_ctx->mutex.lock();
        //qApp->processEvents();
        //qDebug("entering encode video slot.");

        if(video_ctx->put_data==false)
        {
            //data->cond.wait(&data->mutex);
        }
        else if(encode_video)
        {
            //encode_video = !write_video_frame(oc, &video_st,NULL);
            encode_video = !write_video_frame(oc, &video_st,video_ctx->data);
            ret = true;
        }
        //qDebug("exiting encode_video_slot.");
        video_ctx->put_data = false;

        //free depending on situation.
        video_ctx->mutex.unlock();
        return ret;
    }
}

bool movie_encoder_thread::get_from_queue(safe_queue *queue, unsigned char *frame, int buffer_size)
{
    bool ret;
    queue->lock.lock();
    if(queue->queue_size == 0)
    {
        fprintf(stderr,"Waiting for data to be filled...\n");
        //queue->filled_cond.wait(&queue->lock); //might have issues
        ret =  false;
    }
    else
    {
        memcpy(frame, queue->data[queue->tail],buffer_size);
        //printf("	ccdata is %d.\n",*queue->data[queue->tail]);
        free(queue->data[queue->tail]);//free the data pointer pointed by data[tail]

        queue->tail++;
        queue->tail = queue->tail%queue_size;
        queue->queue_size--;
        queue->emptied_cond.wakeOne();
        ret = true;
    }
    queue->lock.unlock();

    return ret;
}

bool movie_encoder_thread::encode_audio_frame()
{
    bool ret = false;

    if(initialized)
    {
        if(my_sound_queue == NULL)
        {
            return false; //hve not written stuff;
        }

        else if(encode_audio)
        {
            if(get_from_queue(my_sound_queue, sound_encode_buffer, nb_samples))
            {
                encode_audio = !write_audio_frame(oc, &audio_st,sound_encode_buffer);
                qDebug("encoding audio frame.");

                ret = true;
            }
            else
            {
                ret = false;
            }
        }
        return ret;
    }
}

void movie_encoder_thread::stopThread()
{

continue_loop = false;
this->wait();          //wait for the thread to finish
qDebug("The encoder thread has successfully finished");

}

void movie_encoder_thread::startThread()
{
        qDebug()<<"The encoder thread is starting!";
        continue_loop = true;
        this->start();
}

//encoder related

void movie_encoder_thread::prepare_new_video_file(char * fname)
{
    memset(&video_st, 0, sizeof(OutputStream));
    memset(&audio_st, 0, sizeof(OutputStream));
    have_video = 0;
    have_audio = 0;
    encode_video = 0;
    encode_audio = 0;
    opt = NULL;
    initialized = false;
    strcpy(filename,fname);
    //filename = fname;

    // allocate the output media context
    avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc)
    {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
    }
    if (!oc)
    {
        printf("Could not setup output context!");
        goto end;

    }
    fmt = oc->oformat;

    // Add the audio and video streams using the default format codecs
    // and initialize the codecs.
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        add_stream(&video_st, oc, &video_codec, fmt->video_codec);
        have_video = 1;
        encode_video = 1;
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
        add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
        have_audio = 1;
        encode_audio = 1;
    }

    /* Now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */
    if (have_video)
        open_video(oc, video_codec, &video_st, opt);
    if (have_audio)
        open_audio(oc, audio_codec, &audio_st, opt);

    av_dump_format(oc, 0, filename, 1);

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            fprintf(stderr, "Could not open '%s': %s\n", filename, av_err2str(ret));
            goto end;
        }
    }
    /* Write the stream header, if any. */
    ret = avformat_write_header(oc, &opt);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: %s\n",av_err2str(ret));
        goto end;
    }
    initialized = true;
end:
    printf("ended \n"); //prolly won't print.
}
bool movie_encoder_thread::cleanup()
{
    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */

    av_write_trailer(oc);

    // Close each codec.
    if (have_video)
        close_stream(oc, &video_st);

    //if (have_audio)
    //{
    //    close_stream(oc, &audio_st);
    //}

    if (!(fmt->flags & AVFMT_NOFILE))
    {
        // Close the output file.
        avio_closep(&oc->pb);
    }

    // free the stream
    avformat_free_context(oc);
    initialized = false;
}

void movie_encoder_thread::add_stream(OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id)
{
    AVCodecContext *c;
    int i;
    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec))
    {
        fprintf(stderr, "Could not find encoder for '%s'\n",avcodec_get_name(codec_id));
        goto end;
    }
    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st)
    {
        fprintf(stderr, "Could not allocate stream\n");
        goto end;
    }
    ost->st->id = oc->nb_streams-1;
    c = avcodec_alloc_context3(*codec);
    if (!c)
    {
        fprintf(stderr, "Could not alloc an encoding context\n");
        goto end;
    }
    ost->enc = c;
    switch ((*codec)->type)
    {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt  = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        c->bit_rate    = 64000;
        c->sample_rate = recording_frequency;

        //c->sample_rate = 44100;
        //c->sample_rate = RECORDING_FREQUENCY;

        c->channels       = av_get_channel_layout_nb_channels(c->channel_layout);
        c->channel_layout = AV_CH_LAYOUT_STEREO;

        if ((*codec)->channel_layouts)
        {
            c->channel_layout = (*codec)->channel_layouts[0];

            for (i = 0; (*codec)->channel_layouts[i]; i++)
            {
                if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                {
                    c->channel_layout = AV_CH_LAYOUT_STEREO;
                }
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        ost->st->time_base = (AVRational){ 1, c->sample_rate };
        break;
    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;
        c->bit_rate = 400000;
        /* Resolution must be a multiple of two. */
        c->width    = 320;
        c->height   = 240;
        /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
        ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
        c->time_base       = ost->st->time_base;
        c->gop_size        = 12; /* emit one intra frame every twelve frames at most */
        c->pix_fmt         = STREAM_PIX_FMT;
        if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO)
        {
            /* just for testing, we also add B-frames */
            c->max_b_frames = 2;
        }
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
        {
            /* Needed to avoid using macroblocks in which some coeffs overflow.
             * This does not happen with normal video, it just happens here as
             * the motion of the chroma plane does not match the luma plane. */
            c->mb_decision = 2;
        }
    break;
    default:
        break;
    }
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
end:
        printf("ended. \n"); //prolly won't print
}

void movie_encoder_thread::open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    int ret;
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;
    av_dict_copy(&opt, opt_arg, 0);
    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);

    if (ret < 0)
    {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        goto end;
    }
    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ost->frame)
    {
        fprintf(stderr, "Could not allocate video frame\n");
        goto end;
    }
    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
/*
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_RGB565BE)
    {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_RGB565BE, c->width, c->height);
        if (!ost->tmp_frame)
        {
            fprintf(stderr, "Could not allocate temporary picture\n");
            goto end;
        }
    }
*/

   ost->tmp_frame = NULL;
   if (c->pix_fmt != AV_PIX_FMT_YUYV422)
   {
       ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUYV422, c->width, c->height);
       if (!ost->tmp_frame)
       {
           fprintf(stderr, "Could not allocate temporary picture\n");
           goto end;
       }
   }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0)
    {
        fprintf(stderr, "Could not copy the stream parameters\n");
        goto end;
    }

end:
    printf("ended. \n"); //prolly won't print
}

void movie_encoder_thread::open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;
    c = ost->enc;

    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);

    if (ret < 0)
    {
        fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
        goto end;
    }
    else
    {
        nb_samples = c->frame_size;
    }

    fprintf(stderr, "number of samples are: %d\n", nb_samples);
    //for this format the number of samples seem to be 1024

    ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,c->sample_rate, nb_samples);

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0)
    {
        fprintf(stderr, "Could not copy the stream parameters\n");
        goto end;
    }
    /* create resampler context */
        ost->swr_ctx = swr_alloc();
        if (!ost->swr_ctx)
        {
            fprintf(stderr, "Could not allocate resampler context\n");
            goto end;
        }

        /* set options */
        av_opt_set_int       (ost->swr_ctx, "in_channel_count",   c->channels,       0);
        av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
        av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
        av_opt_set_int       (ost->swr_ctx, "out_channel_count",  c->channels,       0);
        av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
        av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);
        /* initialize the resampling context */
        if ((ret = swr_init(ost->swr_ctx)) < 0)
        {
            fprintf(stderr, "Failed to initialize the resampling context\n");
            goto end;
        }
end:
        printf("ended!"); //prolly won't print
}

AVFrame *movie_encoder_thread::alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame *picture;
    int ret;
    picture = av_frame_alloc();
    if (!picture)
    {
        return NULL;
    }
    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0)
    {
        fprintf(stderr, "Could not allocate frame data.\n");
        goto end;
    }
end:
    return picture;
}

AVFrame *movie_encoder_thread::alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    int ret;

    if (!frame)
    {
        fprintf(stderr, "Error allocating an audio frame\n");
        goto end;
    }

    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples)
    {
        ret = av_frame_get_buffer(frame, 0);

        if (ret < 0)
        {
            fprintf(stderr, "Error allocating an audio buffer\n");
            goto end;
        }
    }
    return frame;
end:
    printf("ended \n"); //prolly won't print
}

int movie_encoder_thread::write_audio_frame(AVFormatContext *oc, OutputStream *ost, unsigned char *data)
{
    AVCodecContext *c;
    AVPacket pkt = { 0 }; // data and size must be 0;
    AVFrame *frame;
    int ret;
    int got_packet;
    int dst_nb_samples;
    av_init_packet(&pkt);
    c = ost->enc;
    frame = get_audio_frame(ost,data);
    if (frame)
    {
        // convert samples from native format to destination codec format, using the resampler
        // compute destination number of samples
        dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,c->sample_rate, c->sample_rate, AV_ROUND_UP);
        av_assert0(dst_nb_samples == frame->nb_samples);

        /* when we pass a frame to the encoder, it may keep a reference to it
         * internally;
         * make sure we do not overwrite it here
         */
        ret = av_frame_make_writable(ost->frame);
        if (ret < 0)
        {
            goto end;
        }
        /* convert to destination format */
        ret = swr_convert(ost->swr_ctx, ost->frame->data, dst_nb_samples,(const uint8_t **)frame->data, frame->nb_samples);

        if (ret < 0)
        {
            fprintf(stderr, "Error while converting\n");
            goto end;
        }
        frame = ost->frame;
        frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
        ost->samples_count += dst_nb_samples;
    }

    ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
    if (ret < 0)
    {
        fprintf(stderr, "Error encoding audio frame: %s\n", av_err2str(ret));
        goto end;
    }
    if (got_packet)
    {
        ret = write_frame(oc, &c->time_base, ost->st, &pkt);
        if (ret < 0)
        {
            fprintf(stderr, "Error while writing audio frame: %s\n",av_err2str(ret));
            goto end;
        }
    }
    return (frame || got_packet) ? 0 : 1;
end:
    printf("ending. \n");//prolly won't print
}

int movie_encoder_thread::write_video_frame(AVFormatContext *oc, OutputStream *ost, unsigned char *data)
{

    qDebug("write_video_frame called.");
    int ret;
    AVCodecContext *c;
    AVFrame *frame;
    int got_packet = 0;
    AVPacket pkt = { 0 };
    c = ost->enc;
    frame = get_video_frame(ost, data);
    av_init_packet(&pkt);
    /* encode the image */
    ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
    if (ret < 0)
    {
        fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
        goto end;
    }
    if (got_packet)
    {
        ret = write_frame(oc, &c->time_base, ost->st, &pkt);
    } else
    {
        ret = 0;
    }
    if (ret < 0)
    {
        fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
        goto end;
    }
    fprintf(stderr, "frame: %d, got_packet: %d \n", frame, got_packet);

    qDebug("write_video_frame about to return.");
    return (frame || got_packet) ? 0 : 1;
end:
    printf("ending. \n");//prolly won't print
}

void movie_encoder_thread::log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}

int movie_encoder_thread::write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;
    /* Write the compressed frame to the media file. */
    log_packet(fmt_ctx, pkt);
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

AVFrame *movie_encoder_thread::get_audio_frame(OutputStream *ost,unsigned char* data) //out own data here
{
    AVFrame *frame = ost->tmp_frame;
    int j, i, v;
    //int16_t *q = (int16_t*)frame->data[0];
    /* check if we want to generate more frames */

    /*if (av_compare_ts(ost->next_pts, ost->enc->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)

        return NULL;
    */
    /*
    for (j = 0; j <frame->nb_samples; j++) {
        v = (int)(sin(ost->t) * 10000);
        for (i = 0; i < ost->enc->channels; i++)
            *q++ = v;
        ost->t     += ost->tincr;
        ost->tincr += ost->tincr2;
    }
    */
    memcpy(frame->data[0],data,frame->nb_samples*ost->enc->channels*2); //S16_two_channels
    //memcpy(frame->data[0],data,4096); //S16_two_channels
    //memset(data,0,1024); //S16_two_channels

    //memset(frame->data[0],0,4096); //S16_two_channels


    frame->pts = ost->next_pts;
    ost->next_pts  += frame->nb_samples;
    return frame;
}

AVFrame *movie_encoder_thread::get_video_frame(OutputStream *ost, unsigned char* data)
{
    qDebug("get video frame called.");

    AVCodecContext *c = ost->enc;

    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if (av_frame_make_writable(ost->frame) < 0)
    {
        fprintf(stderr,"could not make frame writable! \n");
        return NULL;
    }

    if (c->pix_fmt != AV_PIX_FMT_YUYV422) //RGB565BE
    {
        if (!ost->sws_ctx)
        {
            ost->sws_ctx = sws_getContext(c->width, c->height,
                                          //AV_PIX_FMT_RGB565BE,
                                          AV_PIX_FMT_YUYV422,
                                          c->width, c->height,
                                          c->pix_fmt,
                                          SCALE_FLAGS, NULL, NULL, NULL);
            if (!ost->sws_ctx)
            {
                fprintf(stderr,"Could not initialize the conversion context\n");
                return NULL;
            }
        }

        //memcpy(ost->tmp_frame->data[0],data,c->width*c->height*2); Something weird is happening here really.

        ost->tmp_frame->data[0] = data;
        sws_scale(ost->sws_ctx, (const uint8_t * const *) ost->tmp_frame->data,
                  ost->tmp_frame->linesize, 0, c->height, ost->frame->data,
                  ost->frame->linesize);
        qDebug("pixfmt different than rgb");

    } else
    {
        //memcpy(ost->frame->data[0],data,c->width*c->height*2); //RGB565BE
        ost->frame->data[0] = data;
       // qDebug("pixfmt is same.");
    }

    //ost->frame->data[0] = data;

    ost->frame->pts = ost->next_pts++;
    qDebug("get video frame about to return.");
    return ost->frame;
}

void movie_encoder_thread::close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}
