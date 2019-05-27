#ifndef PSEUDO_CAMERA_DRIVER_H
#define PSEUDO_CAMERA_DRIVER_H

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <QDebug>


class pseudo_camera_driver
{
public:
    pseudo_camera_driver();
    ~pseudo_camera_driver();
    int init();
    int read_register(int reg);
    int write_register(int reg, int val);
    int store_current_settings_to_file();
    int load_settings_from_file();
    int set_camera();
    int print_settings();
private:
    int file;
    char filename[40];
    const char *buffer;

};

#endif // PSEUDO_CAMERA_DRIVER_H


