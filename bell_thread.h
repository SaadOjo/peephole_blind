#ifndef BELL_THREAD_H
#define BELL_THREAD_H

#include <QThread>
#include <QMutex>
#include <QDebug>
#include <QFile>
#include <QString>
#include <alsa/asoundlib.h>
#include "math.h"

#include "program_state.h"

class bell_thread: public QThread
{
    Q_OBJECT
public:
    bell_thread(QObject *parent = 0, program_state * pstate = NULL);
    ~bell_thread();

    //void stopThread(); //check if initialized
    //void startThread();

    void ring();

protected:
    //void run();

signals:

private:

    program_state *my_program_state;

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


};

#endif // BELL_THREAD_H
