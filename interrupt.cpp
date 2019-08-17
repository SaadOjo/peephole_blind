#include "interrupt.h"

interrupt::interrupt(QObject *parent) : QThread(parent)
{

}


void interrupt::run(){

    int f;
    char *gpio_dir = "/sys/class/gpio/gpio6";
    char gpio_value_path [128];

    sprintf(gpio_value_path, "%s/value", gpio_dir);
    f = open(gpio_value_path, O_RDONLY);
    if (f == -1) {
        printf("Failed to open %s\n", gpio_value_path);
        close(f);
    }
    else
    {
        struct pollfd poll_fds;
        int ret;
        char value[4];
        int n;


        for (int i = 1; i < 100; i++)
        {

            f = open(gpio_value_path, O_RDONLY);

            poll_fds.fd = f;
            poll_fds.events = POLLPRI | POLLERR;
            poll_fds.revents = 0;

            printf("Waiting\n");

            ret = poll(&poll_fds, 1, -1);
            if (ret > 0) {
                int size_of_value = sizeof(value);
                n = read(f, &value, size_of_value);
                printf("read %d bytes, value=%c\n", n, value[0]);
            }

            ret = poll(&poll_fds, 1, -1);
            if (ret > 0) {
                int size_of_value = sizeof(value);
                n = read(f, &value, size_of_value);
                printf("read %d bytes, value=%c\n", n, value[0]);
            }

            emit edge();
            close(f);
            usleep(1000000);

        }
    }

}


interrupt::~interrupt()
{

}

void interrupt::stopThread()
{

continue_loop = false;
this->wait();          //wait for the thread to finish
qDebug("The XX thread has successfully finished");

}

void interrupt::startThread()
{
        qDebug()<<"The XX thread is starting!";
        continue_loop = true;
        this->start();
}
