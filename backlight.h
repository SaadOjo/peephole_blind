#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include <QThread>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


class backlight
{
public:
    backlight();
    ~backlight();
    void turn_on();
    void turn_off();

public slots:
    void turn_on_slot();
    void turn_off_slot();

private:
    void set_brightness(int val);
};

#endif // BACKLIGHT_H
