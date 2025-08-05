#include <QApplication>

#include "videowatcher.h"

int main(int argc,char **argv)
{
    QApplication app(argc,argv);
    VideoWatcher w;
    w.showWindowAtLastSizeAndPosition();
    return app.exec();
}
