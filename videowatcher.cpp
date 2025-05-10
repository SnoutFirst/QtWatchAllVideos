#include "videowatcher.h"

#include <QMovie>
#include <QLabel>
#include <QNetworkReply>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QWindow>
#include <QScreen>

#define MIN_MS_TO_SHOW_GIF_LOOPING_IF_NECESSARY (3 * 1000)

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
    menu->addAction("Shuffled", this, [this]{m_PlayMode = PlayMode::Shuffled; startLoad();})->setShortcut(QKeySequence("Ctrl+S"));
    menu->addAction("Reversed", this, [this]{m_PlayMode = PlayMode::Reversed; startLoad();})->setShortcut(QKeySequence("Ctrl+R"));
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

    m_GifLabel = new QLabel;
    m_GifLabel->setAlignment(Qt::AlignCenter);
    m_GifLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    m_GifLabel->setStyleSheet("background:black");

    m_Player = new QMediaPlayer(this);

    m_GraphicsScene     = new QGraphicsScene(this);
    m_VideoGraphicsItem = new QGraphicsVideoItem;
    m_GraphicsScene->addItem(m_VideoGraphicsItem);
    m_GifLabelProxy  = m_GraphicsScene->addWidget(m_GifLabel);

    m_ControlsPanel      = new ControlsPanel;
    m_ControlsPanelProxy = m_GraphicsScene->addWidget(m_ControlsPanel);
    m_ControlsPanelProxy->setZValue(999);

    m_VideoGraphicsItem->setVisible(false);
    m_GifLabelProxy ->setVisible(false);

    m_Player = new QMediaPlayer(this);
    m_Player->setVideoOutput(m_VideoGraphicsItem);

    m_GraphicsView = new MyGraphicsView(m_GraphicsScene);
    m_GraphicsView->onResized = [this]{ resizeGraphicsItems(); };
    m_GraphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_GraphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_GraphicsView->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    m_GraphicsView->setFrameStyle(QFrame::NoFrame);
    m_GraphicsView->setAlignment(Qt::AlignCenter);
    m_GraphicsView->installEventFilter(this);
    m_GraphicsView->hide();

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
    mainLayout->addWidget(m_GraphicsView, 1);
    setLayout(mainLayout);
}

void VideoWatcher::watchAllVideosForwards()
{
    m_PlayMode = PlayMode::Forward;
    startLoad();
}

void VideoWatcher::connectSignals()
{
    connect(m_UrlInput, &QLineEdit::returnPressed, this, &VideoWatcher::watchAllVideosForwards);
    connect(m_WatchAllBtn, &QToolButton::clicked, this, &VideoWatcher::watchAllVideosForwards);
    connect(m_PrevBtn, &QPushButton::clicked, this, &VideoWatcher::playPrevious);
    connect(m_NextBtn, &QPushButton::clicked, this, &VideoWatcher::playNext);

    connect(m_WebView, &QWebEngineView::loadFinished, this, &VideoWatcher::onPageLoaded);
    connect(m_Player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s)
    {
        if(s == QMediaPlayer::EndOfMedia) playNext();
    });

    connect(m_ControlsPanel, &ControlsPanel::playPauseToggled, this, &VideoWatcher::handlePlayPauseToggled);
    connect(this, &VideoWatcher::playPauseToggleHandled, m_ControlsPanel, &ControlsPanel::updatePlayPauseIcon);
}

void VideoWatcher::startLoad()
{
    QUrl url = QUrl::fromUserInput(m_UrlInput->text().trimmed());
    if(!url.isValid())
    {
        if(!url.isEmpty())
            m_LoadingLabel->setText("Invalid URL");
        return;
    }
    m_LoadingLabel->setText("Loading...");
    m_GraphicsView->hide();
    m_WebView->show();
    m_VideoUrls.clear();
    m_CurrentIndex = 0;
    m_WebView->load(url);
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
    m_GraphicsView->setVisible(true);
    m_GraphicsView->setFocus();
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
    if(!m_GifCurrentFrame.isNull() && m_GifLabelProxy->isVisible())
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
        m_VideoGraphicsItem->hide();

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

            m_GifMovie = new QMovie(reply, QByteArray(), this);
            reply->setParent(m_GifMovie); // QMovie takes ownership
            m_GifMovie->setCacheMode(QMovie::CacheAll);
            m_GifLabel->setMovie(m_GifMovie);
            m_GifLabelProxy->show();
            connect(m_GifMovie, &QMovie::frameChanged, this, [this, first = true](int frame) mutable
            {
                if(!m_GifMovie)
                    return;
                if(frame == 0)
                {
                    if(first)
                    {
                        //frameCount() is 0 for the entire first loop (maybe only for certain gif types?), so we need this hack to detect right after first loop
                        first = false;
                    }
                    else if(m_GifElapsedTimer.elapsed() >= MIN_MS_TO_SHOW_GIF_LOOPING_IF_NECESSARY)
                    {
                        playNext();
                        return; //don't show the first frame twice
                    }
                }
                m_GifCurrentFrame = m_GifMovie->currentPixmap();
                scaleAndPresentGifCurrentFrame();
            });
            m_GifMovie->start();
            m_GifElapsedTimer.restart();
        });
    }
    else
    {
        m_GifLabelProxy->hide();
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
        m_VideoGraphicsItem->show();
        m_Player->play();
    }
    saveLastPosition();
    m_ControlsPanelProxy->setVisible(true);
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
    QString url = m_WebView->url().toString(QUrl::RemoveFragment|QUrl::StripTrailingSlash);
    QVariantMap value;
    value["videoUrl"] = m_VideoUrls[m_CurrentIndex];
    value["date"] = QDateTime::currentSecsSinceEpoch(); // Add this line
    m_Settings.setValue("last:" + url, value);
}

void VideoWatcher::maybeCleanupOldState()
{
    if(QRandomGenerator::global()->bounded(100) != 0)
        return; //1% chance

    const qint64 oneYearAgo = QDateTime::currentSecsSinceEpoch() - (60LL * 60 * 24 * 365);

    for(const QString &key : m_Settings.allKeys())
    {
        if(key.startsWith("last:"))
        {
            QVariantMap value = m_Settings.value(key).toMap();
            if(value.contains("date"))
            {
                qint64 date = value["date"].toLongLong(); //if the conversion fails, cleanup that entry :-P (date will be 0)
                if(date < oneYearAgo)
                    m_Settings.remove(key);
            }
        }
    }
}

void VideoWatcher::loadLastPosition()
{
    if(m_PlayMode != PlayMode::Forward) return;
    maybeCleanupOldState();
    QString key = "last:" + m_WebView->url().toString(QUrl::RemoveFragment|QUrl::StripTrailingSlash);
    QVariantMap value = m_Settings.value(key).toMap();
    if(!value.contains("videoUrl"))
        return;
    int index = m_VideoUrls.indexOf(value["videoUrl"].toString());
    if(index >= 0)
        m_CurrentIndex = index;
}

void VideoWatcher::toggleFullscreenWidget(QWidget *widget)
{
    if(widget->isFullScreen())
    {
        widget->setParent(this);
        widget->showNormal();
        layout()->addWidget(widget);
        widget->setFocus();
        return;
    }

    // --- entering full-screen ---
    widget->setParent(nullptr);
    widget->setWindowFlag(Qt::Window);

    widget->winId(); // forces native window + handle
    QScreen *currentScreen = windowHandle()->screen();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    widget->setScreen(currentScreen);
#else
    if(QWindow *w = widget->windowHandle())
        w->setScreen(currentScreen);
#endif

    widget->move(currentScreen->geometry().topLeft());
    widget->resize(currentScreen->size());
    widget->showFullScreen();
}

void VideoWatcher::toggleFullscreen()
{
    toggleFullscreenWidget(m_GraphicsView);
}

bool VideoWatcher::eitherIsFullscreen()
{
    //return m_VideoWidget->isFullScreen() || m_GifLabel->isFullScreen();
    return m_GraphicsView->isFullScreen();
}

bool VideoWatcher::handleKey(QKeyEvent *e)
{
    switch(e->key())
    {
    case Qt::Key_M: m_Player->setMuted(!m_Player->isMuted()); return true;
    case Qt::Key_Space:
        handlePlayPauseToggled();
        return true;
    case Qt::Key_N:
        [[fallthrough]];
    case Qt::Key_Return:
        [[fallthrough]];
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
    if(obj == m_GraphicsView && event->type() == QEvent::MouseButtonDblClick)
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
void VideoWatcher::resizeEvent(QResizeEvent *e)
{
    scaleAndPresentGifCurrentFrame();
    QWidget::resizeEvent(e);
}
void VideoWatcher::resizeGraphicsItems()
{
    const QSizeF viewSize = m_GraphicsView->viewport()->size();

    m_GraphicsScene->setSceneRect(QRectF(QPointF(0, 0), viewSize));

    m_VideoGraphicsItem->setPos(0, 0);
    m_VideoGraphicsItem->setSize(viewSize);

    m_GifLabel->resize(viewSize.toSize());
    m_GifLabelProxy->setPos(0, 0);

    const int panelHeight = m_ControlsPanel->sizeHint().height();
    m_ControlsPanel->setFixedWidth(viewSize.width());
    m_ControlsPanelProxy->setPos(0, viewSize.height() - panelHeight);
}
void VideoWatcher::handlePlayPauseToggled()
{
    bool playing = true;
    if(m_GifLabel->isVisible() && m_GifMovie)
    {
        switch(m_GifMovie->state())
        {
        case QMovie::NotRunning:
            [[fallthrough]];
        case QMovie::Paused:
            m_GifMovie->setPaused(false);
            playing = true;
            break;
        case QMovie::Running:
            m_GifMovie->setPaused(true);
            playing = false;
            break;
        }
    }
    else if(m_VideoGraphicsItem->isVisible() && m_Player)
    {
        switch(m_Player->state())
        {
        case QMediaPlayer::StoppedState:
            [[fallthrough]];
        case QMediaPlayer::PausedState:
            m_Player->play();
            playing = true;
            break;
        case QMediaPlayer::PlayingState:
            m_Player->pause();
            playing = false;
            break;
        }
    }
    emit playPauseToggleHandled(playing);
}
