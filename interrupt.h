#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <QThread>
#include <QMutex>
#include <QDebug>
#include <QFile>
#include <QDebug>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>



class interrupt: public QThread
{
    Q_OBJECT
public:
    interrupt(QObject *parent = 0);
    ~interrupt();


    void stopThread(); //check if initialized
    void startThread();

protected:
    void run();

signals:
    void edge();

private:
    bool continue_loop;
};

#endif // INTERRUPT_H


