#include <QApplication>
#include <QString>
#include <string_view>

#include "internal_use_only/config.hpp"

#include "videowatcher.h"

int main(int argc, char **argv)
{
    // Keep the original application name: QStandardPaths::AppDataLocation (the
    // per-thread resume history, watcher.ini) and the default QSettings store
    // (window geometry) are derived from it, so carrying on with the old name
    // means an existing install keeps its history and window position after the
    // move into the cmake_template repository. Only the displayed name changes.
    QApplication::setApplicationName(QStringLiteral("QtWatchAllVideos"));
    QApplication::setApplicationDisplayName(QStringLiteral("QtWatchAllVideos"));
    QApplication::setApplicationVersion(QString::fromUtf8(QtWatchAllVideos::cmake::project_version.data(),
                                                          static_cast<qsizetype>(QtWatchAllVideos::cmake::project_version.size())));

    QApplication app(argc, argv);
    VideoWatcher w;
    w.showWindowAtLastSizeAndPosition();
    return app.exec();
}
