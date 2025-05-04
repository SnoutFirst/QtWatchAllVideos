#include "myvideowidget.h"
#include <QCoreApplication>
#include <QWidget>

void MyVideoWidget::keyPressEvent(QKeyEvent *e)
{
    if(isFullScreen() && (e->key() == Qt::Key_Escape || e->key() == Qt::Key_F))
    {
        if(onExitFullscreen) onExitFullscreen();
        e->accept();
        return;
    }

    if(QWidget *p = parentWidget())
    {
        QCoreApplication::sendEvent(p, e);
        if(e->isAccepted()) return;
    }

    QVideoWidget::keyPressEvent(e);
}

void MyVideoWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    if(isFullScreen() && onExitFullscreen)
    {
        onExitFullscreen();
        e->accept();
        return;
    }

    if(QWidget *p = parentWidget())
    {
        QCoreApplication::sendEvent(p, e);
        if(e->isAccepted()) return;
    }

    QVideoWidget::mouseDoubleClickEvent(e);
}
