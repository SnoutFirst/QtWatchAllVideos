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
    m_PlayPauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_PlayPauseBtn->setIconSize(QSize(32, 32));
    m_PlayPauseBtn->setFlat(false);
    m_PlayPauseBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(0,0,0,128);"
        "  color: white;"
        "  border: none;"
        "}"
        "QPushButton:hover { background-color: rgba(30,30,30,160); }"
        "QPushButton:pressed { background-color: rgba(0,0,0,200); }"
    );
    lay->addStretch();
    lay->addWidget(m_PlayPauseBtn);
    lay->addStretch();

    connect(m_PlayPauseBtn,&QPushButton::clicked,this,&ControlsPanel::playPauseToggled);
}
