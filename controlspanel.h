#pragma once

#include <QWidget>

class QPushButton;

class ControlsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ControlsPanel(QWidget *parent = nullptr);
private:
    QPushButton *m_PlayPauseBtn{};
signals:
    void playPauseToggled();
public slots:
    void updatePlayPauseIcon(bool playing);
};
