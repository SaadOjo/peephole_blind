#ifndef TOUCH_DETECTOR_H
#define TOUCH_DETECTOR_H

#include <QObject>
#include <QEvent>

class touch_detector: public QObject
{
    Q_OBJECT
public:
    touch_detector();
protected:
    bool eventFilter(QObject *obj, QEvent *event);
signals:
    void touch_detected_signal();
};


#endif // TOUCH_DETECTOR_H

