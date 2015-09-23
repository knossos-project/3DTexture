#include "widget.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QSurfaceFormat format;
    format.setMajorVersion(2);
    format.setMinorVersion(0);
    //format.setSwapBehavior(QSurfaceFormat::SingleBuffer);
    format.setProfile(QSurfaceFormat::NoProfile);
    format.setOption(QSurfaceFormat::DebugContext);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    widget widget;
    widget.show();
    return app.exec();
}
