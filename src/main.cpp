#include "MainWindow.hpp"

#include "logging/logger/custom_logger/custom_logger.h"
#include "logging/logging.h"

int main (int argc, char *argv[])
{
    LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>());

    LoggerFramework::LogEx lg("main");
    LOG_DEBUG(lg) << "Start SC-Deutsch-Launcher";

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/logo.png"));
    MainWindow w;
    w.show();

    return a.exec();
}
