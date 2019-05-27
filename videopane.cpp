#include "videopane.h"
#include <QApplication>

videopane::videopane(QWidget *parent) :
    QWidget(parent)
{
    setAutoFillBackground(true);
}


void videopane::paintEvent(QPaintEvent *) {


    QPainter painter(this);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 30));
    painter.drawText(rect(), Qt::AlignCenter, "Qt");
    painter.drawPixmap(this->rect(),pixmap);
    //painter.drawPixmap(((QWidget*)parent())->rect(),pixmap);
}


void videopane::setPicture(image_with_mutex *i){

    i->mutex.lock();
    pixmap=QPixmap::fromImage(*i->image);
    i->mutex.unlock();
    update();
    qApp->processEvents();
}


videopane::~videopane(){

}




