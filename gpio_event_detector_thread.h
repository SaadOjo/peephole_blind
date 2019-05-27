#ifndef GPIO_EVENT_DETECTOR_THREAD_H
#define GPIO_EVENT_DETECTOR_THREAD_H

#include <QThread>
#include <QMutex>
#include <QDebug>
#include <QFile>
#include <QString>


class gpio_event_detector_thread: public QThread
{
    Q_OBJECT
public:
    gpio_event_detector_thread(QObject *parent = 0, int gpio_pin = 0);
    ~gpio_event_detector_thread();


    void stopThread(); //check if initialized
    void startThread();

protected:
    void run();

signals:
    void rising_edge();
    void falling_edge();

private:
    bool continue_loop;

    bool last_level;
    bool current_level;
    int _gpio_pin;
};

#endif // FAKE_INTERRUPT_H

