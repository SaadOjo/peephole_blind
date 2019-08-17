#include "backlight.h"

backlight::backlight()
{
}
backlight::~backlight()
{
}
void backlight::turn_on()
{
    set_brightness(6);
}
void backlight::turn_off()
{
    set_brightness(0);
}
void backlight::set_brightness(int val)
{
    int fd;
    if(val < 9 && val>= 0)
    {
        fd=open("/sys/devices/soc0/backlight.9/backlight/backlight.9/brightness",O_WRONLY,S_IRUSR);
         if(fd==-1)
         {
             printf("Backlight value file not found.\n");
         }
         else
         {
             char c[2];
             c[0] = val+'0';
             c[1] = '/0';
             write(fd,c,1);
             close(fd);

         }


    }


}

void backlight::turn_on_slot()
{
    turn_on();
}
void backlight::turn_off_slot()
{
    turn_off();
}

