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
    QVideoWidget   *m_VideoWidget{};
    QMediaPlayer   *m_Player{};

    // State
    QStringList     m_VideoUrls;
    int             m_CurrentIndex{0};
    PlayMode        m_PlayMode{PlayMode::Forward};
    QSettings       m_Settings;

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
protected:
    void keyPressEvent(QKeyEvent *e) override;
};

#endif // VIDEOWATCHER_H
