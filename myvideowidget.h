#pragma once

#include <QVideoWidget>
#include <QKeyEvent>
#include <QMouseEvent>
#include <functional>

class MyVideoWidget : public QVideoWidget
{
    Q_OBJECT

public:
    using QVideoWidget::QVideoWidget;

    std::function<void()> onExitFullscreen;

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
};
