#include "touch_detector.h"

touch_detector::touch_detector()
{
}
bool touch_detector::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        emit touch_detected_signal();
        return false;
    }
    else if(event->type() == QEvent::MouseButtonRelease) {
        emit touch_released_signal();
        return false;
    }
    else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
}
