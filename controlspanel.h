#pragma once

#include <QWidget>

class QPushButton;

class ControlsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ControlsPanel(QWidget *parent = nullptr);

signals:
    void playPauseToggled();

private:
    QPushButton *m_PlayPauseBtn{};
};
