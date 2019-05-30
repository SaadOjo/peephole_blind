#ifndef AUDIO_CAPTURE_THREAD_H
#define AUDIO_CAPTURE_THREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTime>
#include <QString>
#include <QDebug>

#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

#include "structures.h"

#define AUDIO_SAMPLES_IN_QUEUE   1536
#define RECORDING_FREQUENCY      8000 //44100  //or 8000
#define AUDIO_CAPTURE_QUEUE_SIZE 24

enum audio_capture_type
{PLAYBACK_QUEUE,
 LISTEN_PLAYBACK_QUEUE,
 ENCODER_QUEUE,
 PLAYBACK_AND_ENCODER_QUEUES};

//Check buffer sizes

class audio_capture_thread : public QThread
{
    Q_OBJECT

public:
    audio_capture_thread(QObject *parent = 0, safe_encode_audio_context* acontext = NULL);
    ~audio_capture_thread();
    void stopThread();
    void startThread(enum audio_capture_type capture_type);
    void flush_playback_queue();
    safe_queue *myQueue,*myEncoderQueue;
    safe_encode_audio_context *my_safe_encode_audio_context;
    start_context  audio_start_context;
    unsigned char *encoder_audio_buffer;

protected:
    void run();
signals:
    void give_encode_audio_context(safe_encode_audio_context*);
    void audio_capture_started_signal(start_context*);
    void audio_capture_stopped_signal();
private:

    bool continue_loop;
    int periods;
    int pcmreturn;
    unsigned int frequency;
    signed int left, right;
    snd_pcm_uframes_t buffer_frames;
    snd_pcm_uframes_t periodsize;
    snd_pcm_t *pcm_handle;
    void init();

    bool store_in_encoder_queue;
    bool store_in_playback_queue;

    bool only_listen;

    void init_queue(safe_queue *queue);
    void put_in_queue(safe_queue *queue, unsigned char *frame);
};

#endif // AUDIO_CAPTURE_THREAD_H
