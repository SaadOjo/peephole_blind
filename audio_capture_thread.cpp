#include "audio_capture_thread.h"

audio_capture_thread::audio_capture_thread(QObject *parent, safe_encode_audio_context* acontext) : QThread(parent)
{

    if(acontext != NULL)
    {
        acontext->mutex.lock();
        my_safe_encode_audio_context = acontext;
        my_safe_encode_audio_context->nb_samples = 0;
        acontext->mutex.unlock();
        //handle when you get a null ptr.
    }

    periods = 2;
    //buffer_frames = (4096*periods)>>2;
    buffer_frames = (AUDIO_SAMPLES_IN_QUEUE*4*periods)>>2;


    //fprintf(stderr ,"The requested frame size is: %d\n",buffer_frames);
    //frequency = 44100;

    frequency = RECORDING_FREQUENCY;

    continue_loop = false;

    myQueue = new(safe_queue);
    myQueue->lock.lock();
    myQueue->data = (unsigned char**)malloc(sizeof(unsigned char*)*AUDIO_CAPTURE_QUEUE_SIZE);
    myQueue->head = 0;
    myQueue->tail = 0;
    myQueue->queue_size = 0;
    myQueue->lock.unlock();

    myEncoderQueue = new(safe_queue);
    myEncoderQueue->lock.lock();
    myEncoderQueue->data = (unsigned char**)malloc(sizeof(unsigned char*)*AUDIO_CAPTURE_QUEUE_SIZE);
    myEncoderQueue->head = 0;
    myEncoderQueue->tail = 0;
    myEncoderQueue->queue_size = 0;
    myEncoderQueue->lock.unlock();


    store_in_encoder_queue = false; //not really needed i suppose at they will certainly be overwritten.
    store_in_playback_queue = false;

    only_listen = false;
}

audio_capture_thread::~audio_capture_thread()
{
    init_queue(myQueue); //using here because frees all the datas.
    myQueue->lock.lock();
    free(myQueue->data);
    myQueue->lock.unlock();

    init_queue(myEncoderQueue); //using here because frees all the datas.
    myEncoderQueue->lock.lock();
    free(myEncoderQueue->data);
    myEncoderQueue->lock.unlock();
}

void audio_capture_thread::init_queue(safe_queue *queue)
{
    //delete all of the data in the audio_queue here

    fprintf(stderr, "Entering initializing queue. \n");

    queue->lock.lock();

    fprintf(stderr, "queue details: head: %d, tail: %d, size: %d \n" ,queue->head, queue->tail, queue->queue_size);

    for(int i = 0 ; i < queue->queue_size; i++)
    {
        fprintf(stderr, "attempting to free: %d \n" ,(queue->tail + i)%AUDIO_CAPTURE_QUEUE_SIZE);
        free(queue->data[(queue->tail + i)%AUDIO_CAPTURE_QUEUE_SIZE]);
    }

    queue->head = 0;
    queue->tail = 0;
    queue->queue_size = 0;
    queue->lock.unlock();

    fprintf(stderr, "Exiting initializing queue. \n");

}
void audio_capture_thread::flush_playback_queue()
{
    init_queue(myQueue);
}

void audio_capture_thread::put_in_queue(safe_queue *queue, unsigned char *frame)
{
    queue->lock.lock();
    if(queue->queue_size >= AUDIO_CAPTURE_QUEUE_SIZE)
    {
        printf("Waiting for data to be emptied...\n");
    }
    else
    {
        fprintf(stderr, "putting data in queue.\n");
        queue->data[queue->head] = frame;
        //printf("head is %d.\n",queue->head);
        queue->head++;
        queue->head = queue->head%AUDIO_CAPTURE_QUEUE_SIZE;
        queue->queue_size++;
        queue->filled_cond.wakeOne(); //added now
        //printf("%d size of the queue is.\n",queue->queue_size);
    }
    queue->lock.unlock();
}

void audio_capture_thread::run(){
// Do main stuff here.

    init(); //can perhaps be moved to the constructor
    init_queue(myQueue);

    my_safe_encode_audio_context->mutex.lock();
    my_safe_encode_audio_context->audio_safe_queue = myEncoderQueue;
    my_safe_encode_audio_context->nb_samples = periodsize; //period size deterimined in init.
    my_safe_encode_audio_context->queue_size = AUDIO_CAPTURE_QUEUE_SIZE;
    my_safe_encode_audio_context->recording_frequency = frequency;
    my_safe_encode_audio_context->mutex.unlock();

    init_queue(myEncoderQueue);

    audio_start_context.mutex.lock();
    audio_start_context.queue = myQueue;
    audio_start_context.buffer_size = periodsize;
    audio_start_context.frequency =  frequency;
    audio_start_context.queue_size = AUDIO_CAPTURE_QUEUE_SIZE;
    audio_start_context.mutex.unlock();

    if(store_in_playback_queue)
    {
        emit audio_capture_started_signal(&audio_start_context);
    }

    while(continue_loop)
    {
        unsigned char* frame = (unsigned char*)malloc(periodsize);
        unsigned char* encoderFrame = (unsigned char*)malloc(periodsize);

        while ((pcmreturn = snd_pcm_readi(pcm_handle, frame, periodsize>>2)) < 0) { //might work, may have to use memcpy instead
          fprintf(stderr, "Ops!\n");
          snd_pcm_prepare(pcm_handle);
          fprintf(stderr, "<<<<<<<<<<<<<<< Buffer Overrun >>>>>>>>>>>>>>>\n");
        }
        //increase capture sound

        for(int i = 0; i<periodsize>>2; i++)
        {
            short left, right;
            left  = frame[i*4    ] | frame[i*4 + 1] << 8;
            right = frame[i*4 + 2] | frame[i*4 + 3] << 8;
            left = left*5;
            right = right*5;

            frame[i*4    ] = left;
            frame[i*4 + 1] = left>>8;
            frame[i*4 + 2] = right;
            frame[i*4 + 3] = right>>8;
        }


        if(store_in_playback_queue)
        {
            if(only_listen)
            {
                for(int i = 0; i<periodsize>>2; i++)
                {
                    frame[i*4 + 2] = 0; //for left or right not sure. will check.
                    frame[i*4 + 3] = 0;
                }
            }

            put_in_queue(myQueue,frame); //already safe
        }
        if(store_in_encoder_queue)
        {
            memcpy(encoderFrame,frame,periodsize);
            put_in_queue(myEncoderQueue,encoderFrame); //already safe
        }

        //qDebug("Period size is: %d \n",periodsize);
    }

    init_queue(myQueue);
    init_queue(myEncoderQueue);

    if(store_in_playback_queue)
    {
        emit audio_capture_stopped_signal();
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    //no need to free pcm handle (already freed.).

}

void audio_capture_thread::init()
{
    fprintf(stderr, "Initializing Microphone! \n");

    unsigned int frequency_exact;
    snd_pcm_uframes_t buffer_frames_exact = buffer_frames;
    snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;
    snd_pcm_hw_params_t *hwparams;
    char *pcm_name;
    pcm_name = strdup("plughw:0,0");
    snd_pcm_hw_params_alloca(&hwparams);

    if (snd_pcm_open(&pcm_handle, pcm_name, stream, 0) < 0) {
      fprintf(stderr, "Error opening Capture PCM device %s\n", pcm_name);
      goto end;
    }
   /* Init hwparams with full configuration space */
    if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
      fprintf(stderr, "Can not configure this PCM device.\n");
      goto end;
    }

    if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
      fprintf(stderr, "Error setting access.\n");
      goto end;
    }

    /* Set sample format */
    if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
      fprintf(stderr, "Error setting format.\n");
      goto end;
    }

    frequency_exact = frequency;
    if (snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &frequency_exact, 0) < 0) {
      fprintf(stderr, "Error setting rate.\n");
      goto end;
    }
    if(frequency_exact != frequency)
    {
    fprintf(stderr, "Desired frequency could not be set, actual frequency is: %d \n", frequency_exact);
    }

    /* Set number of channels */
    if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 2) < 0) {
      fprintf(stderr, "Error setting channels.\n");
      goto end;
    }

    /* Set number of periods. Periods used to be called fragments. */
    if (snd_pcm_hw_params_set_periods(pcm_handle, hwparams, periods, 0) < 0) {
      fprintf(stderr, "Error setting periods.\n");
      goto end;
    }


    /* Set buffer size (in frames). The resulting latency is given by */
    /* latency = periodsize * periods / (rate * bytes_per_frame)     */
    if (snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hwparams, &buffer_frames_exact) < 0) {
      fprintf(stderr, "Error setting buffersize.\n");
      goto end;
    }
    if(buffer_frames!=buffer_frames_exact)
    {
        printf("The desired buffer size of %d could not be set, \n going with nearest size of  %d \n",buffer_frames,buffer_frames_exact);
    }
    buffer_frames = buffer_frames_exact; //Make class property.
    periodsize = (buffer_frames << 2)/periods; // Might give errors for some

      /* Apply HW parameter settings to */
    /* PCM device and prepare device  */
    if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
      fprintf(stderr, "Error setting HW params.\n");
      goto end;
    }

end:
    printf("end of init \n"); //will probably not print.
    //Free memory here
    //free(hwparams);
    //free(pcm_name);

}

void audio_capture_thread::change_capture_type(enum audio_capture_type capture_type)
{
    if(this->isRunning())
    {
        switch(capture_type)
        {
            case LISTEN_PLAYBACK_AND_ENCODER_QUEUES:
            only_listen = true;
            store_in_playback_queue = true;
            store_in_encoder_queue  = true;
            emit audio_capture_started_signal(&audio_start_context);
            break;
            case PLAYBACK_AND_ENCODER_QUEUES:
            only_listen = false;
            store_in_playback_queue = true;
            store_in_encoder_queue  = true;
            emit audio_capture_started_signal(&audio_start_context);
            break;
            case PLAYBACK_QUEUE_OFF:
            store_in_playback_queue = false;
            emit audio_capture_stopped_signal();
            break;
            default:
            break;

        }
    }
}

void audio_capture_thread::stopThread()
{
    continue_loop = false; // allow the run command finish by ending while
    this->wait();          //wait for the thread to finish
    qDebug("The audio thread has successfully finished");
}

void audio_capture_thread::startThread(enum audio_capture_type capture_type)
{
    //fprintf(stderr, "audio_capture_type %d. \n", capture_type);
    //while(1);
    switch(capture_type)
    {
    case PLAYBACK_QUEUE:
        only_listen = false;
        store_in_playback_queue = true;  //thread safe??
        store_in_encoder_queue  = false;
        break;
    case LISTEN_PLAYBACK_QUEUE:
        only_listen = true;
        store_in_playback_queue = true;
        store_in_encoder_queue  = false;
        break;
    case ENCODER_QUEUE:
        store_in_playback_queue = false;
        store_in_encoder_queue  = true;
        break;
    case LISTEN_PLAYBACK_AND_ENCODER_QUEUES:
        only_listen = true;
        store_in_playback_queue = true;
        store_in_encoder_queue  = true;
        break;
    case PLAYBACK_AND_ENCODER_QUEUES:
        only_listen = false;
        store_in_playback_queue = true;
        store_in_encoder_queue  = true;
        break;
    }

    qDebug()<<"The audio thread is starting!";
    continue_loop = true;
    this->start();
}

/*
if(context_data_filled_atleast_once == 0)
{
    encoder_audio_buffer =(unsigned char*)malloc(periodsize); //memory leak possible as all the data might not be freed by the encoder
    memcpy(encoder_audio_buffer, temp_audio_buffer, periodsize); //use wait condition

    my_safe_encode_audio_context.mutex.lock();
    my_safe_encode_audio_context.data = encoder_audio_buffer;
    my_safe_encode_audio_context.put_data = true;
    my_safe_encode_audio_context.mutex.unlock(); //more like a wait condition here just to make sure all data is freed,

    emit give_encode_audio_context(&my_safe_encode_audio_context);
    context_data_filled_atleast_once = 1;
}
else
{
    my_safe_encode_audio_context.mutex.lock();  //audio encoding is taking too long.
    memcpy(encoder_audio_buffer, temp_audio_buffer, periodsize); //use wait condition
    my_safe_encode_audio_context.put_data = true;
    my_safe_encode_audio_context.cond.wakeOne();
    my_safe_encode_audio_context.mutex.unlock(); //more like a wait condition here just to make sure all data is freed,
}
*/
