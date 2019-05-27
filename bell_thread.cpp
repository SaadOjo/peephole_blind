#include "bell_thread.h"

bell_thread::bell_thread(QObject *parent, program_state * pstate) : QThread(parent)
{
    pstate->mutex.lock();
    my_program_state = pstate;
    pstate->mutex.unlock();
}

bell_thread::~bell_thread()
{
}

void bell_thread::ring()
{
    init();

    unsigned char buffer[queue_buffer_size];
    memset(buffer,0,queue_buffer_size);

    int bell_sound_number;
    my_program_state->mutex.lock();
    bell_sound_number = my_program_state->settings_state.bell_sound;
    my_program_state->mutex.unlock();

    char bell_name[40];
    sprintf(bell_name, "bell_%d.wav" ,bell_sound_number);

    int filedesc = open(bell_name, O_RDONLY);
    int pcm = 1;
    if(filedesc < 0)
    {
        fprintf(stderr, "sound file could not be found, exiting. \n");
    }
    else
    {
       while(pcm > 0)
       {
           //fprintf(stderr, "Bleh!\n");
           //sleep(1);
            fprintf(stderr, "looping.. \n");

            if (pcm = read(filedesc, buffer, queue_buffer_size) > 0)
            {
                while ((pcmreturn = snd_pcm_writei(pcm_handle, buffer , queue_buffer_size>>2)) < 0) //make adjustable
                {
                 snd_pcm_prepare(pcm_handle);
                 //add wait condition here
                fprintf(stderr, "<<<<<<<<<<<<<<< Buffer Underrun >>>>>>>>>>>>>>>\n");
                }
            }

       }

       close(filedesc);
    }

    //free stuff before ending
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);

}

void bell_thread::init()
{

    fprintf(stderr, "Initialising bell! \n");
    queue_buffer_size = 4096;
    periods = 2;
    buffer_frames = (queue_buffer_size>>2)*periods;
    frequency     = 44100;

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

/*
    for(int i = 0; i<queue_buffer_size>>2;i++)
    {
        short int sound;
        sound = 5000*sin(i*440*3.14/frequency);
        buffer[i*4    ] = sound;
        buffer[i*4 + 1] = sound >> 8;

        buffer[i*4 + 2] = sound;
        buffer[i*4 + 3] = sound >> 8;

    }
*/
