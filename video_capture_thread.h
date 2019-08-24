#ifndef CAPTURETHREAD_H
#define CAPTURETHREAD_H

#include <QThread>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>
#include <QCoreApplication> //for process events
#include <QTime>
#include <QImageWriter>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <libv4l2.h>
#include "structures.h"
#include "pseudo_camera_driver.h"
#include "program_state.h"


#include "pxp.h"

#define FRAME_WIDTH  320
#define FRAME_HEIGHT 240
#define FPS_AVG_OVER 10

enum color_space
{
    YUV422,
    RGB16,
    RGB24
};


#define CLEAR(x) memset(&(x), 0, sizeof(x))


class video_capture_thread : public QThread
{
    Q_OBJECT

public:
    video_capture_thread(QObject *parent = 0, safe_encode_video_context* vcontext = NULL, program_state* pstate = NULL ); //dont know what is best default for program state
    ~video_capture_thread();

    void stopThread();
    void startThread();
    void set_camera_color_space(enum color_space cspace);
    void set_take_photos_flag();

    image_with_mutex *myIWM;
    image_with_mutex *messageIWM;


protected:
    void run();
signals:
    void renderedImage(image_with_mutex *image);
    //void give_encode_video_context(safe_encode_video_context*);

private:
    safe_encode_video_context  *my_safe_encode_video_context;

    void take_and_save_photo();
    void show_image();
    void show_message(int message);


    unsigned char* image_buffer;
    unsigned char* rgb_image_buffer;
    unsigned char* encoder_buffer;

    QTime video_time;

    QImage *image;

    struct buffer
    {
            void   *start;
            size_t length;
    };

    struct v4l2_format              fmt;
    struct v4l2_buffer              buf;
    struct v4l2_requestbuffers      req;
    enum v4l2_buf_type              type;
    fd_set                          fds;
    struct timeval                  tv;
    int                             r, fd;
    unsigned int                    i, n_buffers;
    char* dev_name;
    char                            out_name[256];
    FILE                            *fout;
    struct buffer                   *buffers;
    bool                            continue_loop;
    bool                            take_photos;
    QTime                           *time;
    unsigned long int               frame_counter;


    enum color_space    current_color_space;

    program_state * video_capture_thread_program_state;

    char filename[50];
    int filenumber;

};

static void xioctl(int fh, int request, void *arg)
{
        int r;

        do {
                r = v4l2_ioctl(fh, request, arg);
        } while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));

        if (r == -1) {
                fprintf(stderr, "error %d, %s\n", errno, strerror(errno));
                exit(EXIT_FAILURE);
        }
}



#endif // CAPTURETHREAD_H
