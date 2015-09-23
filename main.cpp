#include "widget.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    widget widget;
    widget.show();
    return app.exec();
}
