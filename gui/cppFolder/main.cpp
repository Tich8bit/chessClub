#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const int mode = MainWindow::startupChoice();
    if (mode < 0) {
        return 0;
    }

    MainWindow window;
    window.applyStartup(mode);
    window.show();

    return app.exec();
}