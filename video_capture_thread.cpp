#include <QString>
#include <QDebug>
#include <QBuffer>
#include <QImage>
#include <QFile>
#include <typeinfo>
#include <iostream>

#include "video_capture_thread.h"

video_capture_thread::video_capture_thread(QObject *parent,safe_encode_video_context* vcontext) : QThread(parent)
{

    current_color_space = RGB16;

    if(vcontext != NULL)
    {
        vcontext->mutex.lock();
        my_safe_encode_video_context = vcontext;
        vcontext->mutex.unlock();
        //handle when you get a null ptr.
    }

    fd = -1;
    dev_name = (char*) "/dev/video1";
    continue_loop = true;
    time = new QTime;
    frame_counter = 0;
    myIWM = new image_with_mutex;


    set_camera_color_space(RGB16);
/*
    my_driver.write_register(0x12,0x14); //resoloution and rgb mode
    my_driver.write_register(0x3D,0x92);
    my_driver.write_register(0x40,0xD0); //RGB565
    my_driver.read_register(0x40); //just set the last bit


    // Setting the color matrix
    //                       reg   val
    my_driver.write_register(0x4F,0x40); //MT1 R
    my_driver.write_register(0x50,0x00); //MT2 R
    my_driver.write_register(0x51,0x00); //MT3 R
    my_driver.write_register(0x52,0x00); //MT4 G
    my_driver.write_register(0x53,0x40); //MT5 G
    my_driver.write_register(0x54,0x00); //MT6 G
    my_driver.write_register(0x55,0x00); //MT7 B
    my_driver.write_register(0x56,0x00); //MT8 B
    my_driver.write_register(0x57,0x40); //MT9 B
    my_driver.write_register(0x58,0b01110111); //MTXS (9:2)
    unsigned char reg_mtxs = my_driver.read_register(0x69); //just set the last bit
    my_driver.write_register(0x69,reg_mtxs&0xFE|0x00);
*/

    //my_driver.write_register(0x41,0x00);

    //while(1);
    //PxP my_pxp;
    //my_pxp.start();

    fd = v4l2_open(dev_name, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0) {
            perror("Cannot open device");
            exit(EXIT_FAILURE);
    }

    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = FRAME_WIDTH;
    fmt.fmt.pix.height      = FRAME_HEIGHT;
//  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; //actually not yuv when registers in camera are manually set to give rgb

    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
    xioctl(fd, VIDIOC_S_FMT, &fmt);
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
//    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) {
//          printf("Libv4l didn't accept YUYV format. Can't proceed.\\n");
            printf("Libv4l didn't accept RGB24 format. Can't proceed.\\n");
            exit(EXIT_FAILURE);
    }
    if ((fmt.fmt.pix.width != FRAME_WIDTH) || (fmt.fmt.pix.height != FRAME_HEIGHT))
            printf("Warning: driver is sending image at %dx%d\\n",
                    fmt.fmt.pix.width, fmt.fmt.pix.height);

    CLEAR(req);
    req.count = 2;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    xioctl(fd, VIDIOC_REQBUFS, &req);

    buffers = (buffer*) calloc(req.count, sizeof(*buffers));
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
            CLEAR(buf);

            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_MMAP;
            buf.index       = n_buffers;

            xioctl(fd, VIDIOC_QUERYBUF, &buf);

            buffers[n_buffers].length = buf.length;
            buffers[n_buffers].start = v4l2_mmap(NULL, buf.length,
                          PROT_READ | PROT_WRITE, MAP_SHARED,
                          fd, buf.m.offset);

            if (MAP_FAILED == buffers[n_buffers].start)
            {
                    perror("mmap");
                    exit(EXIT_FAILURE);
            }
    }

    for (i = 0; i < n_buffers; ++i) //enqueue buffer
    {
            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            xioctl(fd, VIDIOC_QBUF, &buf);
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    xioctl(fd, VIDIOC_STREAMON, &type);

    image_buffer =  (unsigned char*) buffers[buf.index].start; //problem found.
    //QImage otherImage(image_buffer, FRAME_WIDTH, FRAME_HEIGHT, QImage::Format_RGB888);

    rgb_image_buffer =(unsigned char*)malloc(320*240*2);
    memset(rgb_image_buffer, 200, 320*240*2);

    encoder_buffer =(unsigned char*)malloc(320*240*2);
    //memset(rgb_image_buffer, 200, 320*240*2);

    image = new QImage((unsigned char*) rgb_image_buffer, FRAME_WIDTH, FRAME_HEIGHT, QImage::Format_RGB16);

    myIWM->mutex.lock(); //use wait condition
    myIWM->image = image;//new QImage();
    myIWM->mutex.unlock(); //use wait condition

    my_safe_encode_video_context->mutex.lock();
    my_safe_encode_video_context->data = encoder_buffer;
    my_safe_encode_video_context->mutex.unlock();

    //emit give_encode_video_context(my_safe_encode_video_context); REMOVE

}

void video_capture_thread::run(){

    QTime time2;

    time->start();

    while(continue_loop)
    {


        frame_counter++;

        //qDebug("frame: %d \n",frame_counter);
        //qDebug("frame%10: %d \n",frame_counter%10);
        //qDebug("frame%1: %d \n",frame_counter%1);


        my_safe_encode_video_context->mutex.lock();
        myIWM->mutex.lock();

        qDebug("video capture thread running.");

        if(frame_counter%FPS_AVG_OVER == 0)
        {
           qDebug("Frame Rate: %0.1f\n",FPS_AVG_OVER*1000.0/time->elapsed());
            time->start();
        }


            do {
                    FD_ZERO(&fds);
                    FD_SET(fd, &fds);

                    // Timeout.
                    tv.tv_sec = 2;
                    tv.tv_usec = 0;

                    r = select(fd + 1, &fds, NULL, NULL, &tv);
            } while ((r == -1 && (errno = EINTR))); //shouldn't it be ==
            if (r == -1) {
                    perror("select");
                    return;
            }

            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            xioctl(fd, VIDIOC_DQBUF, &buf); //uncommented (this might have been the issue)

            image_buffer = (unsigned char*)buffers[buf.index].start; //useless if image_buffer is used as data for QImage. must be same actual memory location.

            //convert the image here.


            //time2.start();
            //rgb_image_buffer = convert from yuv422 to rgb565le
            unsigned char pix, lb, hb;

            switch(current_color_space)
            {
            case RGB16:
                for(int i = 0; i<fmt.fmt.pix.sizeimage/2 ;i++)
                {
                    //pix = *(image_buffer + i*2 );

                    //*(rgb_image_buffer + i*2)     = pix<<3&0xe0|pix>>3;
                    //*(rgb_image_buffer + i*2 + 1) = pix&0xf8|pix>>5;

                    //*(rgb_image_buffer + i*2 + 1) = 0b11111000; //high bytes working fine RRRRRR
                    //*(rgb_image_buffer + i*2)     = 0b00000000; //low bytes

                    *(rgb_image_buffer + i*2 + 1) = *(image_buffer + i*2 );
                    *(rgb_image_buffer + i*2)     = *(image_buffer + i*2 + 1);

                     //*(rgb_image_buffer + i*2 ) = *(image_buffer + i*2 ); //processor loading
                     //*(rgb_image_buffer + i*2 + 1)     = *(image_buffer + i*2 + 1);
                }

                break;

            case YUV422:

                for(int i = 0; i<fmt.fmt.pix.sizeimage/2; i++)
                {
                    pix = image_buffer[i*2];

                    *(rgb_image_buffer + i*2)     = pix<<3&0xe0|pix>>3;
                    *(rgb_image_buffer + i*2 + 1) = pix&0xf8|pix>>5;

                }
                break;

            }

            //fprintf(stderr, "time taken for the conversion is: %0.3f \n",(float)time2.elapsed()/1000.0);

            //image_buffer = rgb_image_buffer;

            //my_pxp.cc_frame_start(image_buffer, fmt.fmt.pix.sizeimage);
            //fprintf(stderr,"The size of the image is :%d. \n", fmt.fmt.pix.sizeimage);
              /*
            unsigned char pix ,lb, ub;
            for(int i = 0; i< 320*240;i++)
            {
                //pix = *(image_buffer + i*2 );

                //*(rgb_image_buffer + i*2)     = pix<<3&0xe0|pix>>3;
                //*(rgb_image_buffer + i*2 + 1) = pix&0xf8|pix>>5;

                //*(rgb_image_buffer + i*2 + 1) = 0b11111000; //high bytes working fine RRRRRR
                //*(rgb_image_buffer + i*2)     = 0b00000000; //low bytes

                *(rgb_image_buffer + i*2 + 1) = *(image_buffer + i*2 );
                *(rgb_image_buffer + i*2)     = *(image_buffer + i*2 + 1);
            }
            */


            //rgb_image_buffer = image_buffer; //just testing
            memcpy(encoder_buffer,image_buffer,fmt.fmt.pix.sizeimage);

        my_safe_encode_video_context->put_data = true;

        //if encoding
        if(my_safe_encode_video_context->is_encoding)
        {
            my_safe_encode_video_context->cond.wait(&my_safe_encode_video_context->mutex);
        }

        myIWM->mutex.unlock(); //use wait condition
        my_safe_encode_video_context->mutex.unlock();

        //qApp->processEvents(); something that can be useful.

        emit renderedImage(myIWM);
        //my_pxp.cc_frame_end();
        //qDebug("ending for now ccframe!");
        //continue_loop = false;
        //usleep(1000000/10);
        xioctl(fd, VIDIOC_QBUF, &buf);
    }
}
void video_capture_thread::set_camera_color_space(enum color_space cspace)
{

    current_color_space = cspace;

    pseudo_camera_driver my_driver; //manually set the camera.
    my_driver.load_settings_from_file();

    if(cspace == RGB16)
    {
        my_driver.write_register(0x12,0x14); //resoloution and rgb mode
        my_driver.write_register(0x3D,0x92);
        my_driver.write_register(0x40,0xD0); //RGB565
        my_driver.read_register(0x40); //just set the last bit


        // Setting the color matrix
        //                       reg   val
        my_driver.write_register(0x4F,0x40); //MT1 R
        my_driver.write_register(0x50,0x00); //MT2 R
        my_driver.write_register(0x51,0x00); //MT3 R
        my_driver.write_register(0x52,0x00); //MT4 G
        my_driver.write_register(0x53,0x40); //MT5 G
        my_driver.write_register(0x54,0x00); //MT6 G
        my_driver.write_register(0x55,0x00); //MT7 B
        my_driver.write_register(0x56,0x00); //MT8 B
        my_driver.write_register(0x57,0x40); //MT9 B
        my_driver.write_register(0x58,0b01110111); //MTXS (9:2)
        unsigned char reg_mtxs = my_driver.read_register(0x69); //just set the last bit
        my_driver.write_register(0x69,reg_mtxs&0xFE|0x00);

    }
/*
    my_driver.write_register(0x12,0x14); //resoloution and rgb mode
    my_driver.write_register(0x3D,0x92);
    my_driver.write_register(0x40,0xD0); //RGB565
    my_driver.read_register(0x40); //just set the last bit


    // Setting the color matrix
    //                       reg   val
    my_driver.write_register(0x4F,0x40); //MT1 R
    my_driver.write_register(0x50,0x00); //MT2 R
    my_driver.write_register(0x51,0x00); //MT3 R
    my_driver.write_register(0x52,0x00); //MT4 G
    my_driver.write_register(0x53,0x40); //MT5 G
    my_driver.write_register(0x54,0x00); //MT6 G
    my_driver.write_register(0x55,0x00); //MT7 B
    my_driver.write_register(0x56,0x00); //MT8 B
    my_driver.write_register(0x57,0x40); //MT9 B
    my_driver.write_register(0x58,0b01110111); //MTXS (9:2)
    unsigned char reg_mtxs = my_driver.read_register(0x69); //just set the last bit
    my_driver.write_register(0x69,reg_mtxs&0xFE|0x00);
*/


}

video_capture_thread::~video_capture_thread()
{
    free(rgb_image_buffer);

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(fd, VIDIOC_STREAMOFF, &type);

    for (i = 0; i < n_buffers; ++i)
    {
        v4l2_munmap(buffers[i].start, buffers[i].length);
    }
    v4l2_close(fd);
}

void video_capture_thread::stopThread()
{
    continue_loop = false;
    this->wait();          // wait for the thread to finish
    qDebug("The video capture thread has successfully finished");
}

void video_capture_thread::startThread()
{
    qDebug()<<"The video capture thread is starting!";
    continue_loop = true;
    this->start();
}
