#include <QtGui/QApplication>
#include <QTranslator>
#include "mainwindow.h"
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QString locale=QLocale::system().name();

    QTranslator *translator = new QTranslator(&a);
    if( translator->load( QString("./ps3muxer_%1").arg(locale) ) )
        a.installTranslator(translator);
    else
        delete translator;

    MainWindow w;
    w.show();
    return a.exec();
}
