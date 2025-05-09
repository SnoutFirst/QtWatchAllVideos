#include "videowatcher.h"

#include <QMovie>
#include <QLabel>
#include <QNetworkReply>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QWindow>
#include <QScreen>

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

    m_WebView = new QWebEngineView;
    m_WebView->setHtml(R"(
        <html><body style='font-family:sans-serif;padding:2em;color:#888'>
        Paste a thread URL above and click <b>Watch All Videos</b>.<br><br>
        Any Cloudflare or CAPTCHA page will appear here before videos are extracted.
        </body></html>
    )");
    m_VideoWidget = new QVideoWidget;
    m_VideoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_VideoWidget->hide();

    m_GifLabel = new QLabel;
    m_GifLabel->setAlignment(Qt::AlignCenter);
    m_GifLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    m_GifLabel->setStyleSheet("background:black");
    m_GifLabel->hide();

    m_Player = new QMediaPlayer(this);
    m_Player->setVideoOutput(m_VideoWidget);

    auto *topBox = new QWidget;
    auto *topLayout = new QHBoxLayout(topBox);
    topLayout->setContentsMargins(4, 4, 4, 4);
    topLayout->setSpacing(4);
    topLayout->addWidget(m_UrlInput);
    topLayout->addWidget(m_WatchAllBtn);
    topLayout->addWidget(m_LoadingLabel);
    topLayout->addWidget(m_PrevBtn);
    topLayout->addWidget(m_NextBtn);
    topLayout->addWidget(m_CountLabel);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(topBox, 0, Qt::AlignTop);
    mainLayout->addWidget(m_WebView, 1);
    mainLayout->addWidget(m_VideoWidget);
    mainLayout->addWidget(m_GifLabel);
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
    static const QStringList exts{ "webm","mp4","wmv","avi","mov","mkv","flv","ogg","m4v","mpg","mpeg","3gp","f4v","swf","asf","rm","vob","ts","m2v","divx","gif" };

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
        if(m_First)
        {
            m_First = false;
            return;
        }
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
void VideoWatcher::scaleAndPresentGifCurrentFrame()
{
    if(!m_GifCurrentFrame.isNull() && m_GifLabel->isVisible())
    {
        QPixmap scaled = m_GifCurrentFrame.scaled(m_GifLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation );
        m_GifLabel->setPixmap(scaled);
    }
}
void VideoWatcher::playCurrent()
{
    if(m_CurrentIndex < 0 || m_CurrentIndex >= m_VideoUrls.size()) return;

    QUrl baseUrl = m_WebView->url();
    QUrl resolved = baseUrl.resolved(QUrl(m_VideoUrls[m_CurrentIndex]));
    QString ext = resolved.fileName().section('.',-1).toLower();

    if(ext == "gif")
    {
        if(m_GifMovie)
        {
            m_GifMovie->stop();
            delete m_GifMovie;
            m_GifMovie = nullptr;
        }
        m_Player->stop();
        m_VideoWidget->hide();

        QNetworkRequest req(resolved);
        req.setRawHeader("User-Agent", m_WebView->page()->profile()->httpUserAgent().toUtf8());
        //req.setRawHeader("Cookie", cookieHeader);
        req.setRawHeader("Referer", m_WebView->url().toString().toUtf8());

        QNetworkReply *reply = m_Nam.get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]
        {
            if(reply->error() != QNetworkReply::NoError)
            {
                qDebug() << "GIF load failed:" << reply->errorString();
                reply->deleteLater();
                return;
            }

            if(m_GifMovie)
            {
                m_GifMovie->stop();
                delete m_GifMovie;
                m_GifMovie = nullptr;
            }
            m_Player->stop();
            m_VideoWidget->hide();
            m_GifLabel->hide();

            m_GifMovie = new QMovie(reply, QByteArray(), this);
            reply->setParent(m_GifMovie); // QMovie takes ownership
            m_GifMovie->setCacheMode(QMovie::CacheAll);
            m_GifLabel->setMovie(m_GifMovie);
            m_GifLabel->show();
            bool first = true;
            connect(m_GifMovie, &QMovie::frameChanged, this, [this, &first](int frame)
            {
                if(!m_GifMovie)
                    return;
                m_GifCurrentFrame = m_GifMovie->currentPixmap();
                scaleAndPresentGifCurrentFrame();
                if(frame == 0)
                {
                    if(first)
                    {
                        //frameCount() is 0 for the entire first loop (maybe only for certain gif types?), so we need this hack to detect right after first loop. TODO: don't show the very first frame twice xD
                        first = false;
                        return;
                    }
                    playNext();
                }
            });
            m_GifMovie->start();
        });
    }
    else
    {
        m_GifLabel->hide();
        if(m_GifMovie)
        {
            m_GifMovie->stop();
            delete m_GifMovie;
            m_GifMovie = nullptr;
        }

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        m_Player->setSource(resolved);
#else
        m_Player->setMedia(resolved);
#endif
        m_VideoWidget->show();
        m_Player->play();
    }
    saveLastPosition();
    updateUi();
}

void VideoWatcher::playNext()
{
    if(m_GifMovie)
        m_GifMovie->stop();
    if(m_CurrentIndex + 1 < m_VideoUrls.size())
    {
        ++m_CurrentIndex;
        playCurrent();
    }
}

void VideoWatcher::playPrevious()
{
    if(m_GifMovie)
        m_GifMovie->stop();
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

void VideoWatcher::toggleFullscreenWidget(QWidget *widget)
{
    if(widget->isFullScreen())
    {
        widget->setParent(this);
        widget->showNormal();
        layout()->addWidget(widget);
        widget->setFocus();
    }
    else //go fullscreen
    {
        widget->setParent(nullptr);
        widget->setWindowFlag(Qt::Window);
        QScreen *currentScreen = windowHandle()->screen();
        widget->windowHandle()->setScreen(currentScreen); //fullscreen on current screen/monitor. without this line it will fullscreen on primary screen/monitor always (since we just de-parented)
        widget->move(currentScreen->geometry().topLeft());
        widget->resize(currentScreen->size());
        widget->showFullScreen();
    }
}

void VideoWatcher::toggleFullscreen()
{
    QWidget *current;
    if(m_GifLabel->isVisible())
        current = m_GifLabel;
    else //videoWidget->isVisible()
        current = m_VideoWidget;
    toggleFullscreenWidget(current);
}

bool VideoWatcher::eitherIsFullscreen()
{
    return m_VideoWidget->isFullScreen() || m_GifLabel->isFullScreen();
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
        if(eitherIsFullscreen())
            toggleFullscreen();
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
void VideoWatcher::resizeEvent(QResizeEvent *event)
{
    scaleAndPresentGifCurrentFrame();
    QWidget::resizeEvent(event);
}
