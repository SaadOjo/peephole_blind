#include "gpio_event_detector_thread.h"

gpio_event_detector_thread::gpio_event_detector_thread(QObject *parent, int gpio_pin) : QThread(parent)
{
    _gpio_pin = gpio_pin;
}

void gpio_event_detector_thread::run(){

    char gpio_path[40];
    sprintf(gpio_path, "/sys/class/gpio/gpio%d/value" ,_gpio_pin);
    QString qstr = QString::fromAscii(gpio_path);
    QFile f(qstr);


    while(continue_loop)
    {
        if (!f.open(QFile::ReadOnly | QFile::Text))
        {
            qDebug("Could not open the file corresponding to the GPIO pin.");
            break;
        }
        QTextStream in(&f);
        in.read(1)=="1" ? current_level = true : current_level = false;

        if(current_level && !last_level)
        {
            emit rising_edge();

        }
        else if(!current_level && last_level)
        {
            emit falling_edge();
        }

        last_level = current_level;

        usleep(10000);
        f.close();
    }

}


gpio_event_detector_thread::~gpio_event_detector_thread()
{

}

void gpio_event_detector_thread::stopThread()
{

continue_loop = false;
this->wait();          //wait for the thread to finish
qDebug("The gpio event detector thread has successfully finished");

}

void gpio_event_detector_thread::startThread()
{
        qDebug()<<"The gpio event detector thread thread is starting!";
        continue_loop = true;
        this->start();
}
