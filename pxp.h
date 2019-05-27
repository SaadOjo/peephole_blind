#ifndef PXP_H
#define PXP_H

#include <QDebug>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/fb.h>
#include <linux/videodev2.h>


#define MAX_V4L2_DEVICE_NR     64

#define DEFAULT_WIDTH	320
#define DEFAULT_HEIGHT	240

#define	V4L2_BUF_NUM	6


static int g_num_buffers;

static struct v4l2_buffer buf_xx2;


struct buffer {
    void *start;
    struct v4l2_buffer buf;
};

struct pxp_control {
    int fmt_idx;
    int vfd;
    char *s0_infile;
    char *s1_infile;
    unsigned int out_addr;
    char *outfile;
    int outfile_state;
    struct buffer *buffers;
    int global_alpha;
    int global_alpha_val;
    int colorkey;
    unsigned int colorkey_val;
    int hflip;
    int vflip;
    int rotate;
    int rotate_pass;
    struct v4l2_rect s0;
    int dst_state;
    struct v4l2_rect dst;
    int wait;
    int screen_w, screen_h;
};

struct pxp_video_format {
    char *name;
    unsigned int bpp;
    unsigned int fourcc;
    enum v4l2_colorspace colorspace;
};

static pxp_video_format pxp_video_formats[] = {
    {
     "24-bit RGB",
     4,
     V4L2_PIX_FMT_RGB24,
     V4L2_COLORSPACE_SRGB,
     },
    {
     "16-bit RGB 5:6:5",
     2,
     V4L2_PIX_FMT_RGB565,
     V4L2_COLORSPACE_SRGB,
     },
    {
     "16-bit RGB 5:5:5",
     2,
     V4L2_PIX_FMT_RGB555,
     V4L2_COLORSPACE_SRGB,
     },
    {
     "YUV 4:2:0 Planar",
     2,
     V4L2_PIX_FMT_YUV420,
     V4L2_COLORSPACE_JPEG,
     },
    {
     "YUV 4:2:2 Planar",
     2,
     V4L2_PIX_FMT_YUV422P,
     V4L2_COLORSPACE_JPEG,
     },
    {
     "UYVY",
     2,
     V4L2_PIX_FMT_UYVY,
     V4L2_COLORSPACE_JPEG,
     },
    {
     "YUYV",
      2,
     V4L2_PIX_FMT_YUYV,
     V4L2_COLORSPACE_JPEG,
     },
    {
     "YUV32",
     4,
     V4L2_PIX_FMT_YUV32,
     V4L2_COLORSPACE_JPEG,
     },
};

#define VERSION	"1.0"
#define MAX_LEN 512
#define DEFAULT_OUTFILE "out.pxp"

/*
#define PXP_RES		0
#define PXP_DST		1
#define PXP_HFLIP	2
#define PXP_VFLIP	3
#define PXP_WIDTH	4
#define PXP_HEIGHT	5
*/

static int find_video_device(void)
{
    char v4l_devname[20] = "/dev/video";
    char index[3];
    char v4l_device[20];
    struct v4l2_capability cap;
    int fd_v4l;
    int i = 0;

    if ((fd_v4l = open(v4l_devname, O_RDWR, 0)) < 0) {
        printf("unable to open %s for output, continue searching "
            "device.\n", v4l_devname);
    }
    if (ioctl(fd_v4l, VIDIOC_QUERYCAP, &cap) == 0) {
        if (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
            printf("Found v4l2 output device %s.\n", v4l_devname);
            return fd_v4l;
        }
    } else {
        close(fd_v4l);
    }

    // continue to search
    while (i < MAX_V4L2_DEVICE_NR) {
        strcpy(v4l_device, v4l_devname);
        sprintf(index, "%d", i);
        strcat(v4l_device, index);

        if ((fd_v4l = open(v4l_device, O_RDWR, 0)) < 0) {
            i++;
            continue;
        }
        if (ioctl(fd_v4l, VIDIOC_QUERYCAP, &cap)) {
            close(fd_v4l);
            i++;
            continue;
        }
        if (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
            printf("Found v4l2 output device %s.\n", v4l_device);
            break;
        }

        i++;
    }

    if (i == MAX_V4L2_DEVICE_NR)
        return -1;
    else
        return fd_v4l;
}



static struct pxp_control *pxp_init()//int argc, char **argv)
{
    struct pxp_control *pxp;
    int opt;
    int tfd, fd_v4l;
    char buf[8];

    if ((fd_v4l = find_video_device()) < 0) {
        printf("Unable open v4l2 output device.\n");
        return NULL;
    }

/*
    // Disable screen blanking
    tfd = open("/dev/tty0", O_RDWR);
    sprintf(buf, "%s", "\033[9;0]");
    write(tfd, buf, 7);
    close(tfd);
*/


    pxp = (pxp_control*)calloc(1, sizeof(struct pxp_control));
    if (!pxp) {
        perror("failed to allocate PxP control object");
        return NULL;
    }

    pxp->vfd = fd_v4l;

    pxp->buffers = (buffer*)calloc(6, sizeof(*pxp->buffers));

    if (!pxp->buffers) {
        perror("insufficient buffer memory");
        return NULL;
    }

    // Init pxp control struct
    pxp->s0_infile = "";//argv[argc - 2];
    pxp->s1_infile = ""; //strcmp(argv[argc - 1], "BLANK") == 0 ? NULL : argv[argc - 1];
    pxp->outfile = (char*)calloc(1, MAX_LEN);
    pxp->outfile_state = 0;
    strcpy(pxp->outfile, DEFAULT_OUTFILE);
    pxp->screen_w = pxp->s0.width = DEFAULT_WIDTH;
    pxp->screen_h = pxp->s0.height = DEFAULT_HEIGHT;
    pxp->fmt_idx = 6;	//YUYV
    pxp->wait = 1;

    pxp->dst_state = 1;
    pxp->dst.left = 0;
    pxp->dst.top =  0;
    pxp->dst.width = DEFAULT_WIDTH;
    pxp->dst.height =DEFAULT_HEIGHT;

    pxp->hflip = 0;
    pxp->vflip = 0;
    pxp->global_alpha = 0;
    pxp->colorkey = 0;
    pxp->outfile_state = 1;
    strncpy(pxp->outfile, "", MAX_LEN);
    pxp->rotate = 0;

    return pxp;

}



static int pxp_check_capabilities(struct pxp_control *pxp)
{
    //qDebug("Entering check capabilities.");
    struct v4l2_capability cap;

    if (ioctl(pxp->vfd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("VIDIOC_QUERYCAP");
        return 1;
    }

    if (!(cap.capabilities &
          (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_OVERLAY))) {
        perror("video output overlay not detected\n");
        return 1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        perror("streaming support not detected\n");
        return 1;
    }

    //qDebug("Satisfies all capabilities.");


    return 0;
}

static int pxp_config_output(struct pxp_control *pxp)
{
    struct v4l2_output output;
    struct v4l2_format format;
    int out_idx = 1; //out_idx is selcted here, ha, or maybe not, not sure what this is

    if (ioctl(pxp->vfd, VIDIOC_S_OUTPUT, &out_idx) < 0) {
        perror("failed to set output");
        return 1;
    }

    output.index = out_idx;


    if (ioctl(pxp->vfd, VIDIOC_ENUMOUTPUT, &output) >= 0) {
        pxp->out_addr = output.reserved[0];
        printf("V4L output %d (0x%08x): %s\n",
               output.index, output.reserved[0], output.name);
    } else {
        perror("VIDIOC_ENUMOUTPUT");
        return 1;
    }

    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = pxp->s0.width;
    format.fmt.pix.height = pxp->s0.height;
    format.fmt.pix.pixelformat = pxp_video_formats[pxp->fmt_idx].fourcc;
    if (ioctl(pxp->vfd, VIDIOC_S_FMT, &format) < 0) {
        perror("VIDIOC_S_FMT output");
        return 1;
    }

    printf("Video input format: %dx%d %s\n", pxp->s0.width, pxp->s0.height,
           pxp_video_formats[pxp->fmt_idx].name);

    return 0;
}


static int pxp_config_buffer(struct pxp_control *pxp)
{
    struct v4l2_requestbuffers req;
    int ibcnt = V4L2_BUF_NUM;
    int i = 0;

    req.count = ibcnt;
    req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_MMAP;
    printf("request count %d\n", req.count);
    g_num_buffers = req.count;

    if (ioctl(pxp->vfd, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        return 1;
    }

    if (req.count < ibcnt) {
        perror("insufficient buffer control memory");
        return 1;
    }

    for (i = 0; i < ibcnt; i++) {
        pxp->buffers[i].buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        pxp->buffers[i].buf.memory = V4L2_MEMORY_MMAP;
        pxp->buffers[i].buf.index = i;

        if (ioctl(pxp->vfd, VIDIOC_QUERYBUF, &pxp->buffers[i].buf) < 0) {
            perror("VIDIOC_QUERYBUF");
            return 1;
        }

        pxp->buffers[i].start = mmap(NULL /* start anywhere */ ,
                         pxp->buffers[i].buf.length,
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED,
                         pxp->vfd,
                         pxp->buffers[i].buf.m.offset);

        if (pxp->buffers[i].start == MAP_FAILED) {
            perror("failed to mmap pxp buffer");
            return 1;
        }
    }
    return 0;
}
static int pxp_config_windows(struct pxp_control *pxp)
{
    struct v4l2_framebuffer fb;
    struct v4l2_format format;
    struct v4l2_crop crop;

    /* Set FB overlay options */
    fb.flags = V4L2_FBUF_FLAG_OVERLAY;

    if (pxp->global_alpha)
        fb.flags |= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
    if (pxp->colorkey)
        fb.flags |= V4L2_FBUF_FLAG_CHROMAKEY;
    if (ioctl(pxp->vfd, VIDIOC_S_FBUF, &fb) < 0) {
        perror("VIDIOC_S_FBUF");
        return 1;
    }

    /* Set overlay source window */
    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
    format.fmt.win.global_alpha = pxp->global_alpha_val;
    format.fmt.win.chromakey = pxp->colorkey_val;
    format.fmt.win.w.left = 0;
    format.fmt.win.w.top = 0;
    format.fmt.win.w.width = pxp->s0.width;
    format.fmt.win.w.height = pxp->s0.height;
    printf("win.w.l/t/w/h = %d/%d/%d/%d\n", format.fmt.win.w.left,
           format.fmt.win.w.top,
           format.fmt.win.w.width, format.fmt.win.w.height);
    if (ioctl(pxp->vfd, VIDIOC_S_FMT, &format) < 0) {
        perror("VIDIOC_S_FMT output overlay");
        return 1;
    }

    /* Set cropping window */
    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
    if (pxp->dst_state) {
        crop.c.left = pxp->dst.left;
        crop.c.top = pxp->dst.top;
        crop.c.width = pxp->dst.width;
        crop.c.height = pxp->dst.height;
    } else {
        if (pxp->rotate_pass) {
            int scale = 16 * pxp->screen_h / pxp->screen_w;
            if (pxp->rotate == 90 || pxp->rotate == 270) {
                crop.c.left = 0;
                crop.c.top = 0;
            }
            crop.c.width = pxp->screen_w * scale / 16;
            crop.c.height = pxp->screen_h * scale / 16;

            crop.c.width = (crop.c.width >> 3) << 3;
            crop.c.height = (crop.c.height >> 3) << 3;
        } else {
            crop.c.left = 0;
            crop.c.top = 0;
            crop.c.width = pxp->s0.width;
            crop.c.height = pxp->s0.height;
        }
    }
    printf("crop.c.l/t/w/h = %d/%d/%d/%d\n", crop.c.left,
           crop.c.top, crop.c.width, crop.c.height);
    if (ioctl(pxp->vfd, VIDIOC_S_CROP, &crop) < 0) {
        perror("VIDIOC_S_CROP");
        return 1;
    }

    return 0;
}

static int pxp_config_controls(struct pxp_control *pxp)
{
    struct v4l2_control vc;

    /* Horizontal flip */
    if (pxp->hflip)
        vc.value = 1;
    else
        vc.value = 0;
    vc.id = V4L2_CID_HFLIP;
    if (ioctl(pxp->vfd, VIDIOC_S_CTRL, &vc) < 0) {
        perror("VIDIOC_S_CTRL");
        return 1;
    }

    /* Vertical flip */
    if (pxp->vflip)
        vc.value = 1;
    else
        vc.value = 0;
    vc.id = V4L2_CID_VFLIP;
    if (ioctl(pxp->vfd, VIDIOC_S_CTRL, &vc) < 0) {
        perror("VIDIOC_S_CTRL");
        return 1;
    }

    /* Rotation */
    vc.id = V4L2_CID_PRIVATE_BASE;
    vc.value = pxp->rotate;
    if (ioctl(pxp->vfd, VIDIOC_S_CTRL, &vc) < 0) {
        perror("VIDIOC_S_CTRL");
        return 1;
    }

    /* Set background color */ //Why are they predefined??
    vc.id = V4L2_CID_PRIVATE_BASE + 1;
    vc.value = 0x0;
    if (ioctl(pxp->vfd, VIDIOC_S_CTRL, &vc) < 0) {
        perror("VIDIOC_S_CTRL");
        return 1;
    }

    /* Set s0 color key */
    vc.id = V4L2_CID_PRIVATE_BASE + 2;
    vc.value = 0xFFFFEE;
    if (ioctl(pxp->vfd, VIDIOC_S_CTRL, &vc) < 0) {
        perror("VIDIOC_S_CTRL");
        return 1;
    }

    return 0;
}

static int start_pxp_convert(struct pxp_control *pxp)
{
    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(pxp->vfd, VIDIOC_STREAMON, &type) < 0) {
        printf("Can't stream on\n");
        return 1;
    }
    return 0;
}

static int pxp_convert_frame_start(struct pxp_control *pxp,unsigned char* yuv_data , int yuv_size)
{

    buf_xx2.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf_xx2.memory = V4L2_MEMORY_MMAP;

    if (ioctl(pxp->vfd, VIDIOC_DQBUF, &buf_xx2) < 0) {
        printf("VIDIOC_DQBUF failed\n");
        return 1;
    }

    memcpy(pxp->buffers[buf_xx2.index].start, yuv_data, yuv_size);

    return 0;

}

static int pxp_convert_frame_end(struct pxp_control *pxp)
{

    if (ioctl(pxp->vfd, VIDIOC_QBUF, &buf_xx2) < 0) {
        printf("VIDIOC_QBUF failed\n");
        return 1;
    }

    return 0;

}

static int pxp_start(struct pxp_control *pxp, unsigned char* yuv_data , int yuv_size)
{
    int i = 0, cnt = 0;
    int fd;
    int s0_size;
    unsigned int total_time;
    struct timeval tv_start, tv_current;
    int ret = 0;

    if (pxp_video_formats[pxp->fmt_idx].fourcc ==  V4L2_PIX_FMT_YUV420)
    {
        s0_size = pxp->s0.width * pxp->s0.height * 3 / 2;
    }
    else
    {
    s0_size = pxp->s0.width * pxp->s0.height
            * pxp_video_formats[pxp->fmt_idx].bpp;
    }


    // Queue buffer
    //if ((fd = open(pxp->s0_infile, O_RDWR, 0)) < 0) {
    //    perror("s0 data open failed");
    //    return 1;
   // }

    printf("PxP processing: start...\n");

    gettimeofday(&tv_start, NULL);

    for (i = 0;i<100 ; i++) {
        /*
        if (quitflag == 1)
            break;
            */

        struct v4l2_buffer buf;
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buf.memory = V4L2_MEMORY_MMAP;
        if (i < g_num_buffers) {
            buf.index = i;
            if (ioctl(pxp->vfd, VIDIOC_QUERYBUF, &buf) < 0) {
                printf("VIDIOC_QUERYBUF failed\n");
                ret = -1;
                break;
            }
        } else {
            if (ioctl(pxp->vfd, VIDIOC_DQBUF, &buf) < 0) {
                printf("VIDIOC_DQBUF failed\n");
                ret = -1;
                break;
            }
        }
        memcpy(pxp->buffers[buf.index].start, yuv_data, yuv_size);
        //cnt = read(fd, pxp->buffers[buf.index].start, s0_size);
        //if (cnt < s0_size)
        //   break;

        if (ioctl(pxp->vfd, VIDIOC_QBUF, &buf) < 0) {
            printf("VIDIOC_QBUF failed\n");
            ret = -1;
            break;
        }


        if (i == 2) {
            int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            if (ioctl(pxp->vfd, VIDIOC_STREAMON, &type) < 0) {
                printf("Can't stream on\n");
                ret = -1;
                break;
            }
        }

    }

    gettimeofday(&tv_current, NULL);
    total_time = (tv_current.tv_sec - tv_start.tv_sec) * 1000000L;
    total_time += tv_current.tv_usec - tv_start.tv_usec;
    printf("total time for %u frames = %u us, %lld fps\n", i, total_time,
           (i * 1000000ULL) / total_time);

    close(fd);
    return ret;
}

static int pxp_stop(struct pxp_control *pxp)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    int i;

    //sleep(pxp->wait);
    printf("complete\n");

    /* Disable PxP */
    if (ioctl(pxp->vfd, VIDIOC_STREAMOFF, &type) < 0) {
        perror("VIDIOC_STREAMOFF");
        return 1;
    }

    for (i = 0; i < g_num_buffers; i++)
        munmap(pxp->buffers[i].start, pxp->buffers[i].buf.length);

    return 0;
}

static int pxp_write_outfile(struct pxp_control *pxp,unsigned char *ccd_frame)
{
//    int fd, ffd, mfd;
    int mfd;

    //struct fb_var_screeninfo var;
    char *out_buf;
    size_t out_buf_size;
    //int n;
    //char fmt_buf[32];
/*
    if ((ffd = open("/dev/fb0", O_RDWR, 0)) < 0) {
        perror("fb device open failed");
        return 1;
    }

    if (ioctl(ffd, FBIOGET_VSCREENINFO, &var)) {
        perror("FBIOGET_VSCREENINFO");
        return 1;
    }

    close(ffd);
*/
    //out_buf_size = var.xres * var.yres * (var.bits_per_pixel >> 3);
    out_buf_size = 320*240*4;

    if ((mfd = open("/dev/mem", O_RDWR, 0)) < 0) {
        perror("mem device open failed");
        return 1;
    }

    out_buf = (char*)mmap(NULL /* start anywhere */ ,
               out_buf_size, PROT_READ, MAP_SHARED, mfd, pxp->out_addr);
    memcpy(ccd_frame,out_buf,out_buf_size);

    if (out_buf == MAP_FAILED) {
        perror("failed to mmap output buffer");
        return 1;
    }
/*
    if ((fd = open(pxp->outfile,
               O_CREAT | O_WRONLY,
               S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
        perror("outfile open failed");
        return 1;
    }

    n = write(fd, out_buf, out_buf_size);
    if (n != out_buf_size) {
        perror("failed to write entire output buffer");
        return 1;
    }

    close(fd);


    if (var.bits_per_pixel == 16)
        strcpy(fmt_buf, "RGB565");
    else
        strcpy(fmt_buf, "RGB24 (32-bit unpacked)");

    printf("Virtual output buffer: %dx%d %s\n",
           var.xres, var.yres, fmt_buf);
   */
    return 0;
}



static void pxp_cleanup(struct pxp_control *pxp)
{
    close(pxp->vfd);
    if (pxp->outfile)
        free(pxp->outfile);
    free(pxp->buffers);
    free(pxp);
}


class PxP
{
public:
    PxP();
    ~PxP();
    int start();
    int stop(unsigned char* ccd_frame);
    int pxp_test();
    int cc_frame_start(unsigned char* image, int size);
    int cc_frame_end(); //not required because we are the ones that queue the buffer.
private:
        struct pxp_control *pxp;
};

#endif // PXP_H
