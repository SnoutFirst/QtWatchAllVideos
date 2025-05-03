#include <QApplication>

#include "videowatcher.h"

int main(int argc,char **argv)
{
    QApplication app(argc,argv);
    VideoWatcher w;
    w.resize(1024,768);
    w.show();
    return app.exec();
}
