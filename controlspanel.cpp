#include "controlspanel.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>

ControlsPanel::ControlsPanel(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(12,6,12,6);

    m_PlayPauseBtn = new QPushButton();
    m_PlayPauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    QSize iconSize(32, 32);
    m_PlayPauseBtn->setIconSize(iconSize);
    m_PlayPauseBtn->setFixedSize(iconSize);
    m_PlayPauseBtn->setFlat(false);
    m_PlayPauseBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: rgba(0,0,0,90);"
        "  color: white;"
        "  border: none;"
        "  border-radius: %1px;"
        "}"
        "QPushButton:hover { background-color: rgba(30,30,30,160); }"
        "QPushButton:pressed { background-color: rgba(0,0,0,200); }"
    ).arg(iconSize.width() / 2));
    lay->addStretch();
    lay->addWidget(m_PlayPauseBtn);
    lay->addStretch();

    connect(m_PlayPauseBtn,&QPushButton::clicked,this,&ControlsPanel::playPauseToggled);
    connect(m_PlayPauseBtn, &QPushButton::clicked, this, [this, playIcon = false]() mutable
    {
        if(playIcon)
        {
            m_PlayPauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        }
        else
        {
            m_PlayPauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        }
        playIcon = !playIcon;
    });
}
