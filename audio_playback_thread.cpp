#include "audio_playback_thread.h"

audio_playback_thread::audio_playback_thread(QObject *parent) : QThread(parent)
{
    continue_loop = false;
}
void audio_playback_thread::act_on_audio_thread_start(start_context* context)
{
    context->mutex.lock();
    myQueue = context->queue;
    queue_buffer_size = context->buffer_size;
    queue_size = context->queue_size;
    frequency =context->frequency;
    context->mutex.unlock();

    startThread();
}
void audio_playback_thread::audio_playback_thread::act_on_audio_thread_stop()
{
    stopThread();
}

void audio_playback_thread::get_from_queue(safe_queue *queue, unsigned char *frame, int buffer_size)
{
    queue->lock.lock();
    if(queue->queue_size == 0)
    {
       // fprintf(stderr,"Waiting for data to be filled...\n");
        queue->filled_cond.wait(&queue->lock); //might have issues
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
    }
    queue->lock.unlock();
}


void audio_playback_thread::run(){

    unsigned char buffer[queue_buffer_size];
    memset(buffer,0,queue_buffer_size);

    init();

    while(continue_loop)
    {
        //fprintf(stderr, "Bleh!\n");
        //sleep(1);

            get_from_queue(myQueue, buffer,queue_buffer_size);
            while ((pcmreturn = snd_pcm_writei(pcm_handle, buffer , queue_buffer_size>>2)) < 0) //make adjustable
            {
             snd_pcm_prepare(pcm_handle);
             //add wait condition here
            fprintf(stderr, "<<<<<<<<<<<<<<< Buffer Underrun >>>>>>>>>>>>>>>\n");
            }
    }

    //free stuff before ending
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    //no need to free pcm_handle.

    qDebug("The audio playback thread has successfully finished. \n");

}

audio_playback_thread::~audio_playback_thread()
{

}


void audio_playback_thread::init()
{

    fprintf(stderr, "Initialising audio playback thread! \n");


    unsigned int frequency_exact;
    snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
    snd_pcm_uframes_t buffer_frames_exact = buffer_frames;
    snd_pcm_hw_params_t *hwparams;
    char *pcm_name;
    pcm_name = strdup("plughw:0,0");
    snd_pcm_hw_params_alloca(&hwparams);

    if (snd_pcm_open(&pcm_handle, pcm_name, stream, 0) < 0) {
      fprintf(stderr, "Error opening Playback PCM device %s\n", pcm_name);
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
    //Handle message in case frequency is not same

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
    frames = periodsize >> 2;

      /* Apply HW parameter settings to */
    /* PCM device and prepare device  */
    if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
      fprintf(stderr, "Error setting HW params.\n");
      goto end;
    }

end:
    printf("the end \n"); // probably will not print anything.

}

void audio_playback_thread::stopThread()
{
     qDebug("starting stopping of audion thread.");
    continue_loop = false; // allow the run command finish by ending while //may need mutex
    this->wait();          //wait for the thread to finish
}

void audio_playback_thread::startThread()
{       periods = 2; //overrides stuff set in on_start_
        //buffer_frames = 1024*periods*2*2; //devide by //(bytespersample*channels)
        //only vaiable thing is the buffer size. rest of the format is fixed.
        //cannot put any random format to the queue.
        //frequency taken from start context.
        buffer_frames = (queue_buffer_size>>2)*periods;
        qDebug()<<"The audio playback thread is starting!";
        continue_loop = true;
        this->start();
}
