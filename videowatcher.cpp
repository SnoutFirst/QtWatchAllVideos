#include "videowatcher.h"

VideoWatcher::VideoWatcher(QWidget *parent)
    : QWidget(parent),
      m_Settings(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/watcher.ini", QSettings::IniFormat)
{
    createUi();
    connectSignals();
}

void VideoWatcher::createUi()
{
    m_UrlInput = new QLineEdit;
    m_WatchAllBtn = new QToolButton;
    m_WatchAllBtn->setText("Watch All Videos");
    m_WatchAllBtn->setPopupMode(QToolButton::MenuButtonPopup);

    QMenu *menu = new QMenu(this);
    menu->addAction("Shuffled", this, [this]{m_PlayMode = PlayMode::Shuffled; startLoad();});
    menu->addAction("Reversed", this, [this]{m_PlayMode = PlayMode::Reversed; startLoad();});
    m_WatchAllBtn->setMenu(menu);

    m_LoadingLabel = new QLabel;
    m_PrevBtn = new QPushButton("Previous");
    m_NextBtn = new QPushButton("Next");
    m_CountLabel = new QLabel;

    m_PrevBtn->setEnabled(false);
    m_NextBtn->setEnabled(false);

    auto *topLayout = new QHBoxLayout;
    topLayout->addWidget(m_UrlInput);
    topLayout->addWidget(m_WatchAllBtn);
    topLayout->addWidget(m_LoadingLabel);
    topLayout->addWidget(m_PrevBtn);
    topLayout->addWidget(m_NextBtn);
    topLayout->addWidget(m_CountLabel);

    m_WebView = new QWebEngineView;
    m_VideoWidget = new QVideoWidget;
    m_VideoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_VideoWidget->hide();

    m_Player = new QMediaPlayer(this);
    m_Player->setVideoOutput(m_VideoWidget);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(m_WebView);
    mainLayout->addWidget(m_VideoWidget);
    setLayout(mainLayout);
}

void VideoWatcher::connectSignals()
{
    connect(m_WatchAllBtn, &QToolButton::clicked, this, [this]{m_PlayMode = PlayMode::Forward; startLoad();});
    connect(m_PrevBtn, &QPushButton::clicked, this, &VideoWatcher::playPrevious);
    connect(m_NextBtn, &QPushButton::clicked, this, &VideoWatcher::playNext);

    connect(m_WebView, &QWebEngineView::loadFinished, this, &VideoWatcher::onPageLoaded);
    connect(m_Player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s)
    {
        if(s == QMediaPlayer::EndOfMedia) playNext();
    });
}

void VideoWatcher::startLoad()
{
    m_LoadingLabel->setText("Loading...");
    m_VideoWidget->hide();
    m_WebView->show();
    m_VideoUrls.clear();
    m_CurrentIndex = 0;
    m_WebView->load(QUrl::fromUserInput(m_UrlInput->text().trimmed()));
}

void VideoWatcher::onPageLoaded(bool ok)
{
    if(!ok)
    {
        m_LoadingLabel->setText("Load failed");
        return;
    }
    m_WebView->page()->toHtml([this](const QString &html){ parseHtml(html); });
}

void VideoWatcher::parseHtml(const QString &html)
{
    static const QStringList exts{ "webm","mp4","wmv","avi","mov","mkv","flv","ogg","m4v","mpg","mpeg","3gp","f4v","swf","asf","rm","vob","ts","m2v","divx" };

    QRegularExpression hrefRe(QString(R"raw(<a[^>]+href\s*=\s*["']([^"']+\.(?:%1))["'])raw").arg(exts.join('|')), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression videoSrcRe(QString(R"raw(<video[^>]+src\s*=\s*["']([^"']+\.(?:%1))["'])raw").arg(exts.join('|')), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression sourceRe(QString(R"raw(<source[^>]+src\s*=\s*["']([^"']+\.(?:%1))["'])raw").arg(exts.join('|')), QRegularExpression::CaseInsensitiveOption);

    auto collect = [&](const QRegularExpression &re)
    {
        QRegularExpressionMatchIterator it = re.globalMatch(html);
        while(it.hasNext())
        {
            QString url = it.next().captured(1);
            m_VideoUrls.append(url);
        }
    };
    collect(hrefRe);
    collect(videoSrcRe);
    collect(sourceRe);

    m_VideoUrls.removeDuplicates();

    if(m_VideoUrls.isEmpty())
    {
        m_LoadingLabel->setText("No videos found – maybe still on CAPTCHA?");
        return;
    }

    transformVideoList();
    m_LoadingLabel->clear();
    m_WebView->hide();
    m_VideoWidget->show();
    m_VideoWidget->setFocus();
    m_VideoWidget->installEventFilter(this);
    loadLastPosition();
    playCurrent();
}

void VideoWatcher::transformVideoList()
{
    if(m_PlayMode == PlayMode::Reversed)
    {
        std::reverse(m_VideoUrls.begin(), m_VideoUrls.end());
    }
    else if(m_PlayMode == PlayMode::Shuffled)
    {
        std::shuffle(m_VideoUrls.begin(), m_VideoUrls.end(), *QRandomGenerator::global());
    }
}

void VideoWatcher::playCurrent()
{
    if(m_CurrentIndex < 0 || m_CurrentIndex >= m_VideoUrls.size()) return;

    QUrl baseUrl = m_WebView->url();
    QUrl resolved = baseUrl.resolved(QUrl(m_VideoUrls[m_CurrentIndex]));
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    m_Player->setSource(resolved);
#else
    m_Player->setMedia(resolved);
#endif
    m_Player->play();
    saveLastPosition();
    updateUi();
}

void VideoWatcher::playNext()
{
    if(m_CurrentIndex + 1 < m_VideoUrls.size())
    {
        ++m_CurrentIndex;
        playCurrent();
    }
}

void VideoWatcher::playPrevious()
{
    if(m_CurrentIndex > 0)
    {
        --m_CurrentIndex;
        playCurrent();
    }
}

void VideoWatcher::updateUi()
{
    m_PrevBtn->setEnabled(m_CurrentIndex > 0);
    m_NextBtn->setEnabled(m_CurrentIndex + 1 < m_VideoUrls.size());
    m_CountLabel->setText(QString("%1 / %2").arg(m_CurrentIndex + 1).arg(m_VideoUrls.size()));
}

void VideoWatcher::saveLastPosition()
{
    if(m_PlayMode != PlayMode::Forward) return;
    QString key = "last:" + m_WebView->url().toString(QUrl::RemoveFragment|QUrl::StripTrailingSlash);
    m_Settings.setValue(key, m_VideoUrls[m_CurrentIndex]);
}

void VideoWatcher::loadLastPosition()
{
    if(m_PlayMode != PlayMode::Forward) return;
    QString key = "last:" + m_WebView->url().toString(QUrl::RemoveFragment|QUrl::StripTrailingSlash);
    QString last = m_Settings.value(key).toString();
    if(last.isEmpty()) return;
    int idx = m_VideoUrls.indexOf(last);
    if(idx >= 0) m_CurrentIndex = idx;
}

void VideoWatcher::toggleFullscreen()
{
    if(m_VideoWidget->isFullScreen())
    {
        m_VideoWidget->setParent(this);
        m_VideoWidget->showNormal();
        layout()->addWidget(m_VideoWidget);
        m_VideoWidget->setFocus();
    }
    else
    {
        m_VideoWidget->setParent(nullptr);
        m_VideoWidget->setWindowFlag(Qt::Window);
        m_VideoWidget->showFullScreen();
    }
}

bool VideoWatcher::handleKey(QKeyEvent *e)
{
    switch(e->key())
    {
    case Qt::Key_M: m_Player->setMuted(!m_Player->isMuted()); return true;
    case Qt::Key_Space:
        m_Player->state() == QMediaPlayer::PlayingState ? m_Player->pause() : m_Player->play(); return true;
    case Qt::Key_N:
    case Qt::Key_Return:
    case Qt::Key_Enter: playNext(); return true;
    case Qt::Key_P: playPrevious(); return true;
    case Qt::Key_F: toggleFullscreen(); return true;
    case Qt::Key_Escape:
        if(m_VideoWidget->isFullScreen()) toggleFullscreen();
        return true;
    default: return false;
    }
}

bool VideoWatcher::eventFilter(QObject *obj, QEvent *event)
{
    if(obj == m_VideoWidget && event->type() == QEvent::MouseButtonDblClick)
    {
        toggleFullscreen();
        return true;
    }

    if(event->type() == QEvent::KeyPress)
    {
        return handleKey(static_cast<QKeyEvent*>(event));
    }

    return QWidget::eventFilter(obj, event);
}

void VideoWatcher::keyPressEvent(QKeyEvent *e)
{
    if(!handleKey(e))
        QWidget::keyPressEvent(e);
}
