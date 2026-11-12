#include <QApplication>

#include "mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("pankop.ch"));
    QApplication::setApplicationName(QStringLiteral("LiniennetzplanEditor"));

    MainWindow w;
    w.show();
    return app.exec();
}
