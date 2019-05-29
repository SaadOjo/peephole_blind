#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //QThread::currentThread()->setPriority(QThread::HighPriority);
    MainWindow w;
    w.showFullScreen();
    //w.show();

    return a.exec();
}
