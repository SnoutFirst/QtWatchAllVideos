#ifndef VIDEOWATCHER_H
#define VIDEOWATCHER_H

#include <QWidget>

#include <QSettings>
#include <QLineEdit>
#include <QToolButton>
#include <QMenu>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <QTimer>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsVideoItem>
#include <QGraphicsProxyWidget>

#include "controlspanel.h"

class QMovie;
class QLabel;

class MyGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    using QGraphicsView::QGraphicsView;
    std::function<void()> onResized;
    std::function<void()> onMouseMoved;

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QGraphicsView::resizeEvent(e);
        if(onResized)
            onResized();
    }
     void mouseMoveEvent(QMouseEvent *e) override
     {
         QGraphicsView::mouseMoveEvent(e);
         if(onMouseMoved)
             onMouseMoved();
     }
     void keyPressEvent(QKeyEvent *event)
     {
         if(isFullScreen())
         {
            emit keyPressed(event);
            event->accept();
         }
         else
            QGraphicsView::keyPressEvent(event);
     }
signals:
     void keyPressed(QKeyEvent *event);
};

class VideoWatcher final : public QWidget
{
    Q_OBJECT

public:
    VideoWatcher(QWidget *parent = nullptr);
    void showWindowAtLastSizeAndPosition();
private:
    enum class PlayMode { Forward, Shuffled, Reversed };

    // UI
    QLineEdit      *m_UrlInput{};
    QToolButton    *m_WatchAllBtn{};
    QLabel         *m_LoadingLabel{};
    QPushButton    *m_PrevBtn{};
    QPushButton    *m_NextBtn{};
    QLabel         *m_CountLabel{};
    QWebEngineView *m_WebView{};
    QGraphicsScene        *m_GraphicsScene;
    MyGraphicsView               *m_GraphicsView;
    QGraphicsVideoItem    *m_VideoGraphicsItem;
    QGraphicsProxyWidget  *m_GifLabelProxy;

    ControlsPanel          *m_ControlsPanel{};
    QGraphicsProxyWidget   *m_ControlsPanelProxy{};
    QTimer   m_MouseIdleTimer;

    QMediaPlayer   *m_Player{};
    QLabel         *m_GifLabel{};
    QMovie         *m_GifMovie = nullptr;
    QPixmap        m_GifCurrentFrame{};
    QElapsedTimer  m_GifElapsedTimer;
    QNetworkAccessManager m_Nam;

    // State
    QStringList     m_VideoUrls;
    int             m_CurrentIndex{0};
    PlayMode        m_PlayMode{PlayMode::Forward};
    QSettings       m_Settings;

    bool m_First = true;

    QAction *m_ActionWatchShuffled = nullptr;
    QAction *m_ActionWatchReversed = nullptr;

    QTimer m_SingleVsDoubleClickTimer;
    // ---------------------------------------------------------------------

    void createUi();
    void connectSignals();
    void startLoad();
    void onPageLoaded(bool ok);
    void parseHtml(const QString &html);
    void transformVideoList();
    void playCurrent();
    void playNext();
    void playPrevious();
    void updateUi();
    void saveLastPosition();
    void loadLastPosition();
    void toggleFullscreen();

    void scaleAndPresentGifCurrentFrame();
    bool eitherIsFullscreen();
    void maybeCleanupOldState();
    void resizeGraphicsItems();

    void saveWindowSizeAndPosition();
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *e) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
signals:
    void aboutToPlay();
    void playPauseStateChanged(bool playing);
private slots:
    bool handleKey(QKeyEvent *e);
    void watchAllVideosForwards();
    void togglePlayPause();

    void handleMouseMoved();
    void handleMouseStoppedMoving();

    void showCustomContextMenu(const QPoint &pos);
};

#endif // VIDEOWATCHER_H
