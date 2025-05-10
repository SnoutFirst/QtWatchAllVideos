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

class QMovie;
class QLabel;

class VideoWatcher final : public QWidget
{
    Q_OBJECT

public:
    VideoWatcher(QWidget *parent = nullptr);

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
    QGraphicsView         *m_GraphicsView;
    QGraphicsVideoItem    *m_VideoGraphicsItem;
    QGraphicsProxyWidget  *m_GifLabelProxy;

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

    bool handleKey(QKeyEvent *e);
    void scaleAndPresentGifCurrentFrame();
    void toggleFullscreenWidget(QWidget *widget);
    bool eitherIsFullscreen();
    void maybeCleanupOldState();
    void resizeGraphicsItems();
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *e) override;
    void resizeEvent(QResizeEvent *event) override;
private slots:
    void watchAllVideosForwards();
};

#endif // VIDEOWATCHER_H
