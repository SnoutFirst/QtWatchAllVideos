#include "controlspanel.h"

#include <QHBoxLayout>
#include <QPushButton>

ControlsPanel::ControlsPanel(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background-color:rgba(0,0,0,128);");

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(12,6,12,6);

    m_PlayPauseBtn = new QPushButton("Play");
    m_PlayPauseBtn->setFlat(true);
    lay->addStretch();
    lay->addWidget(m_PlayPauseBtn);
    lay->addStretch();

    connect(m_PlayPauseBtn,&QPushButton::clicked,this,&ControlsPanel::playPauseToggled);
}
