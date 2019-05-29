#ifndef STRUCTURES_H
#define STRUCTURES_H

#include<QMutex>
#include<QImage>
#include<QWaitCondition>

struct image_with_mutex
{
    QMutex mutex;
    QImage *image;
};

struct safe_queue
{
unsigned char **data;
unsigned int head;
unsigned int tail;
unsigned int  queue_size;

QMutex lock;
QWaitCondition filled_cond, emptied_cond; //according to signal
};

struct start_context
{
    QMutex        mutex;
    safe_queue    *queue;
    int           buffer_size;
    int           queue_size;
    int           frequency;
};

struct safe_encode_video_context
{
    QMutex        mutex;
    unsigned char *data;
    bool          put_data;
    bool          is_encoding;
    QWaitCondition cond;
};

struct safe_encode_audio_context //not a big fan of the naming.
{
    QMutex            mutex;
    int               nb_samples;
    int               queue_size;
    int               recording_frequency;
    struct safe_queue *audio_safe_queue;
    //unsigned char     *data;
    //bool              put_data;
    QWaitCondition    cond;
};

#endif // STRUCTURES_H
