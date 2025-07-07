#include "MainWindow.hpp"

int main (int argc, char *argv[])
{
    QApplication a(argc, argv);
    QIcon icon(":/logo.png");
    if(icon.isNull())
    {
        qDebug() << "Icon ist leer!";
    }
    else
    {
        qDebug() << "Icon geladen.";
    }
    a.setWindowIcon(icon);

    MainWindow w;
    w.show();

    return a.exec();
}
