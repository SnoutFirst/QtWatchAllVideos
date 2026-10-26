#include "videowatcher.h"

#include <QMovie>
#include <QLabel>
#include <QNetworkReply>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QWindow>
#include <QScreen>
#include <QApplication>
#include <QClipboard>

#define MIN_MS_TO_SHOW_GIF_LOOPING_IF_NECESSARY (3 * 1000)

VideoWatcher::VideoWatcher(QWidget *parent)
    : QWidget(parent),
      m_Settings(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/watcher.ini", QSettings::IniFormat)
{
    createUi();
    connectSignals();

    m_MouseIdleTimer.setSingleShot(true);
    m_MouseIdleTimer.setInterval(1500);
    connect(&m_MouseIdleTimer, &QTimer::timeout, this, &VideoWatcher::handleMouseStoppedMoving);

    m_SingleVsDoubleClickTimer.setSingleShot(true);
    m_SingleVsDoubleClickTimer.setInterval(QApplication::doubleClickInterval());
    connect(&m_SingleVsDoubleClickTimer, &QTimer::timeout, this, &VideoWatcher::togglePlayPause);

    m_PlayNextAutomaticallyAndUserInputRaceConditionDetectionElapsedTimer.start();
}
void VideoWatcher::showWindowAtLastSizeAndPosition()
{
    QSettings settings;
    settings.beginGroup("Window");
    if(!settings.contains("size"))
    {
        //first run = show maximized
        showMaximized();
    }
    else
    {
        resize(settings.value("size", QSize(640, 480)).toSize());
        move(settings.value("pos", QPoint(200, 200)).toPoint());
        show();
    }
    settings.endGroup();
}
void VideoWatcher::createUi()
{
    m_UrlInput = new QLineEdit;
    m_WatchAllBtn = new QToolButton;
    m_WatchAllBtn->setText("Watch All Videos");
    m_WatchAllBtn->setPopupMode(QToolButton::MenuButtonPopup);
    m_UrlInput->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_UrlInput, &QLineEdit::customContextMenuRequested, this, &VideoWatcher::showCustomContextMenu);

    m_ActionWatchShuffled = new QAction("Shuffled", this);
    m_ActionWatchReversed = new QAction("Reversed", this);

    m_ActionWatchShuffled->setShortcut(QKeySequence("Ctrl+S"));
    m_ActionWatchReversed->setShortcut(QKeySequence("Ctrl+R"));

    connect(m_ActionWatchShuffled, &QAction::triggered, this, [this]
    {
        m_PlayMode = PlayMode::Shuffled;
        startLoad();
    });
    connect(m_ActionWatchReversed, &QAction::triggered, this, [this]
    {
        m_PlayMode = PlayMode::Reversed;
        startLoad();
    });

    QMenu *menu = new QMenu(this);
    menu->addAction(m_ActionWatchShuffled);
    menu->addAction(m_ActionWatchReversed);
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
        Built with 4chan <b>/gif/</b> browsing in mind, but it works on any site that serves
        its videos as plain <b>&lt;video&gt;</b> elements or as links ending in a video
        extension (including <b>.gif</b>).<br><br>
        It does <b>not</b> work on sites that use fancy JavaScript players to load their
        videos &mdash; <b>YouTube</b> and many others are out of luck.<br><br>
        Any Cloudflare or CAPTCHA page will appear here first; pass it by hand and the
        videos will be extracted from the page you land on.
        </body></html>
    )");

    m_GifLabel = new QLabel;
    m_GifLabel->setAlignment(Qt::AlignCenter);
    m_GifLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

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
    m_GraphicsView->onMouseMoved = [this]{ handleMouseMoved(); };
    m_GraphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_GraphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_GraphicsView->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    m_GraphicsView->setFrameStyle(QFrame::NoFrame);
    m_GraphicsView->setAlignment(Qt::AlignCenter);
    m_GraphicsView->setFocusPolicy(Qt::StrongFocus);
    m_GraphicsView->setMouseTracking(true);
    m_GraphicsView->viewport()->setMouseTracking(true);
    m_GraphicsView->viewport()->installEventFilter(this);
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

    m_GifLabel->setStyleSheet("background:black");
    m_GraphicsScene->setBackgroundBrush(Qt::black);
    m_GraphicsView->setStyleSheet("background-color: black;");
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
    connect(m_NextBtn, &QPushButton::clicked, this, &VideoWatcher::playNextFromUserInput);

    connect(m_WebView, &QWebEngineView::loadFinished, this, &VideoWatcher::onPageLoaded);
    connect(m_Player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s)
    {
        if(s == QMediaPlayer::EndOfMedia)
            playNextAutomatically();
    });

    connect(m_GraphicsView, &MyGraphicsView::keyPressed, this, [this](QKeyEvent *e)
    {
        //keyPressed comes from MyGraphicsView::keyPressEvent(), which decides between
        //consuming the key and passing it on to QGraphicsView by looking at isAccepted().
        //The signal itself drops handleKey()'s return value, so accept here.
        if(handleKey(e))
            e->accept();
    });

    connect(m_ControlsPanel, &ControlsPanel::playPauseToggleButtonClicked, this, &VideoWatcher::togglePlayPause);
    connect(this, &VideoWatcher::playPauseStateChanged, m_ControlsPanel, &ControlsPanel::updatePlayPauseIcon);
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
    loadLastPosition();
    playCurrent();
    handleMouseMoved();
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
QUrl VideoWatcher::resolvedCurrentUrl() const
{
    if(m_CurrentIndex < 0 || m_CurrentIndex >= m_VideoUrls.size())
        return QUrl();
    return m_WebView->url().resolved(QUrl(m_VideoUrls[m_CurrentIndex]));
}

void VideoWatcher::seekBy(qint64 deltaMs)
{
    if(m_VideoUrls.isEmpty())
        return;

    // GIFs have no seekable timeline: QMovie plays the frames on its own clock and the
    // data is streamed in as it is decoded, so there is no position to jump to.
    if(resolvedCurrentUrl().fileName().section('.', -1).toLower() == "gif")
        return;

    const qint64 duration = m_Player->duration();   // 0 until the media is loaded
    const qint64 position = m_Player->position();
    qint64 target = position + deltaMs;

    // Seeking past the end is really a "next", and the next video starts at 0 anyway.
    // playNextFromUserInput() (not playNext()) so the same race guard that protects the
    // N key applies here: just after an auto-advance, a stray keypress must not skip a
    // video the user has not seen.
    if(deltaMs > 0 && duration > 0 && target >= duration)
    {
        playNextFromUserInput();
        return;
    }

    // Before the start of the video: clamp to 0 instead of handing the player a
    // negative position; likewise never seek beyond a known duration.
    if(target < 0)
        target = 0;
    if(duration > 0 && target > duration)
        target = duration;
    m_Player->setPosition(target);

    // A video that has run to its end sits in StoppedState on its last frame; seeking
    // back into it should resume playback rather than leave a frozen picture.
    if(m_Player->state() == QMediaPlayer::StoppedState)
        m_Player->play();
}

void VideoWatcher::playCurrent()
{
    if(m_CurrentIndex < 0 || m_CurrentIndex >= m_VideoUrls.size()) return;

    emit aboutToPlay();

    QUrl resolved = resolvedCurrentUrl();
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

        //connect to aboutToPlay in order to abort the get request if the user triggers next/previous action before the get completes
        connect(this, &VideoWatcher::aboutToPlay, reply, &QNetworkReply::abort);

        connect(reply, &QNetworkReply::finished, this, [this, reply]
        {
            if(reply->error() == QNetworkReply::OperationCanceledError)
            {
                //user triggered next/previous before the get() finished, so we canceled it using the signal aboutToPlay connected to abort. do nothing here just deleteLater the reply and return
                reply->deleteLater();
                return;
            }
            else if(reply->error() != QNetworkReply::NoError)
            {
                qDebug() << "GIF load failed:" << reply->errorString();
                reply->deleteLater();
                return;
            }

            //the user did not trigger next/previous before get() completed, so we don't need to abort() anymore. this disconnect might not be needed since we're already *in* finished(), but it feels right to get rid of it
            disconnect(this, &VideoWatcher::aboutToPlay, reply, &QNetworkReply::abort);

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
                        playNextAutomatically();
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
    updateUi();
    m_GraphicsView->setFocus();
    emit playPauseStateChanged(true);
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

void VideoWatcher::playNextFromUserInput()
{
    if(m_PlayNextAutomaticallyAndUserInputRaceConditionDetectionElapsedTimer.elapsed() > 1000)
    {
        playNext();
    }
    else
    {
        //do nothing
        //detected race condition where user pressed "next" 0.0000001 seconds after the video ended and auto-played "next", which would cause essentially skipping a video. crucially the user can still spam click 'next' as fast as they want and it'll still work
    }
}

void VideoWatcher::playNextAutomatically()
{
    m_PlayNextAutomaticallyAndUserInputRaceConditionDetectionElapsedTimer.start();
    playNext();
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
    value["date"] = QDateTime::currentSecsSinceEpoch();
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

void VideoWatcher::toggleFullscreen()
{
    if(m_GraphicsView->isFullScreen())
    {
        m_GraphicsView->setParent(this);
        m_GraphicsView->showNormal();
        layout()->addWidget(m_GraphicsView);
        m_GraphicsView->setFocus();
        return;
    }

    // --- entering full-screen ---
    m_GraphicsView->setParent(nullptr);
    m_GraphicsView->setWindowFlag(Qt::Window);

    m_GraphicsView->winId(); // forces native window + handle
    QScreen *currentScreen = windowHandle()->screen();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_GraphicsView->setScreen(currentScreen);
#else
    if(QWindow *w = m_GraphicsView->windowHandle())
        w->setScreen(currentScreen);
#endif

    m_GraphicsView->move(currentScreen->geometry().topLeft());
    m_GraphicsView->resize(currentScreen->size());
    m_GraphicsView->showFullScreen();
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
        togglePlayPause();
        return true;
    case Qt::Key_N:
        [[fallthrough]];
    case Qt::Key_Return:
        [[fallthrough]];
    case Qt::Key_Enter: playNextFromUserInput(); return true;
    case Qt::Key_P: playPrevious(); return true;
    case Qt::Key_Left: seekBy(-5000); return true;
    case Qt::Key_Right: seekBy(5000); return true;
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
    if(obj == m_GraphicsView->viewport())
    {
        if(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)
        {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            QPointF scenePos = m_GraphicsView->mapToScene(me->pos());
            QGraphicsItem *item = m_GraphicsScene->itemAt(scenePos, QTransform());
            if(item && (item == m_ControlsPanelProxy || m_ControlsPanelProxy->isAncestorOf(item)))
                return false;

            if(event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton)
            {
                m_SingleVsDoubleClickTimer.start();
            }
            else if(event->type() == QEvent::MouseButtonDblClick)
            {
                m_SingleVsDoubleClickTimer.stop();
                toggleFullscreen();
                return true;
            }
        }
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
void VideoWatcher::closeEvent(QCloseEvent *event)
{
    saveWindowSizeAndPosition();
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
void VideoWatcher::saveWindowSizeAndPosition()
{
    QSettings settings;
    settings.beginGroup("Window");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
}
void VideoWatcher::togglePlayPause()
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
    emit playPauseStateChanged(playing);
}
void VideoWatcher::handleMouseMoved()
{
    m_ControlsPanelProxy->show();
    m_GraphicsView->viewport()->unsetCursor();
    m_MouseIdleTimer.start();
}
void VideoWatcher::handleMouseStoppedMoving()
{
    m_ControlsPanelProxy->hide();
    m_GraphicsView->viewport()->setCursor(Qt::BlankCursor);
}
void VideoWatcher::showCustomContextMenu(const QPoint &pos)
{
    std::unique_ptr<QMenu> contextMenu(m_UrlInput->createStandardContextMenu());

    QAction *pasteAndWatch = new QAction("Paste and Watch All Videos", contextMenu.get());
    connect(pasteAndWatch, &QAction::triggered, this, [this]
    {
        m_UrlInput->setText(QApplication::clipboard()->text().trimmed());
        watchAllVideosForwards();
    });

    // Clone Shuffled and Reversed actions with modified text, but keep same shortcut and slot
    QAction *shuffledClone = new QAction("Paste and Watch All Videos (Shuffled)", contextMenu.get());
    QAction *reversedClone = new QAction("Paste and Watch All Videos (Reversed)", contextMenu.get());

    shuffledClone->setShortcut(m_ActionWatchShuffled->shortcut());
    reversedClone->setShortcut(m_ActionWatchReversed->shortcut());

    connect(shuffledClone, &QAction::triggered, this, [this]
    {
        m_UrlInput->setText(QApplication::clipboard()->text().trimmed());
        m_ActionWatchShuffled->trigger();
    });

    connect(reversedClone, &QAction::triggered, this, [this]
    {
        m_UrlInput->setText(QApplication::clipboard()->text().trimmed());
        m_ActionWatchReversed->trigger();
    });

    contextMenu->addSeparator();
    contextMenu->addAction(pasteAndWatch);
    contextMenu->addAction(shuffledClone);
    contextMenu->addAction(reversedClone);

    contextMenu->exec(m_UrlInput->mapToGlobal(pos));
}
