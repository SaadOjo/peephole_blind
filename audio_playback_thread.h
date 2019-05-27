#ifndef AUDIO_PLAYBACK_THREAD_H
#define AUDIO_PLAYBACK_THREAD_H

#include <QThread>
#include <QImage>
#include <QTime>
#include <QMutex>
#include <QDebug>
#include <assert.h>
#include <alsa/asoundlib.h>

//will have problems if the source and dest have differnent sample rates.

#define __STDC_FORMAT_MACROS
#define __STDC_CONSTANT_MACROS // for UINT64_C

#include "structures.h"

class audio_playback_thread : public QThread
{
    Q_OBJECT

public:
    audio_playback_thread(QObject *parent = 0);
    ~audio_playback_thread();

    void stopThread();
    void startThread();

protected:
    void run();
signals:

private slots:
    void act_on_audio_thread_start(start_context*); //take the queue pointer
    void act_on_audio_thread_stop();

private:

    bool continue_loop;

    safe_queue *myQueue;

    //Alsa related stuff
    int periods;
    int pcmreturn;
    int frames;

    int queue_buffer_size;
    int queue_size;
    unsigned int frequency;
    signed int left, right;
    snd_pcm_uframes_t buffer_frames;
    snd_pcm_uframes_t periodsize;
    snd_pcm_t *pcm_handle;

    void init();
    void get_from_queue(safe_queue *queue, unsigned char *frame, int buffer_size);

};

#endif // AUDIO_PLAYBACK_THREAD_H
