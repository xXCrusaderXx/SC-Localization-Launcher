#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QTranslator>

#include <iostream>

#include "frontend/BackendBridge.hpp"
#include "frontend/CommandLine.hpp"
#include "frontend/SingleInstance.hpp"
#include "logging/logger/custom_logger/custom_logger.h"
#include "logging/logging.h"

int main (int argc, char *argv[])
{
    // --check, --update, --version: no window and no display needed, e.g. for a systemd timer
    if(CommandLine::requested(argc, argv))
    {
        LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>(false));
        QCoreApplication app(argc, argv);
        app.setApplicationVersion(SC_VERSION);
        return CommandLine(SC_APP_ID, SC_VERSION, std::cout, std::cerr).run(app.arguments());
    }

    LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>());

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/logo.png"));
    app.setApplicationVersion(SC_VERSION);
    // Lets Wayland compositors match the window to the installed .desktop file and its icon
    QGuiApplication::setDesktopFileName(SC_APP_ID);

    // The UI is written in English; a built-in translation for the system language replaces it
    QTranslator translator;
    if(translator.load(QLocale(), "sc-localization-launcher", "_", ":/i18n")) app.installTranslator(&translator);

    // A second start only brings the running launcher to the front, before any backend work starts
    // After an update the old launcher is still quitting; wait for it instead of activating it
    SingleInstance instance(SC_APP_ID, app.arguments().contains("--after-update") ? 10000 : 100);
    if(!instance.isPrimary() && instance.activatePrimary()) return 0;
    // No answer: the lock is left over from a launcher that is gone, e.g. a native one seen from the Flatpak
    if(!instance.isPrimary() && !instance.takeOver()) return 1;

    LoggerFramework::LogEx lg("main");
    LOG_DEBUG(lg) << "Start SC Localization Launcher " << SC_VERSION;

    // Declared before the engine, so QML is torn down before the backend
    BackendBridge backend;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.addImportPath("qrc:/qt/qml");
    engine.load(QUrl("qrc:/qt/qml/SCLauncher/Main.qml"));

    if(!engine.rootObjects().isEmpty())
    {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        QObject::connect(&instance, &SingleInstance::activationRequested, window,
                         [window]
                         {
                             window->showNormal();
                             window->raise();
                             window->requestActivate();
                         });
    }

    // Dev helper: SC_SCREENSHOT=<file.png> saves the window after startup and quits
    if(const QString screenshot = qEnvironmentVariable("SC_SCREENSHOT"); !screenshot.isEmpty() && !engine.rootObjects().isEmpty())
    {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        // SC_SCREENSHOT_SETTINGS=1 opens the settings panel first
        if(qEnvironmentVariableIsSet("SC_SCREENSHOT_SETTINGS"))
            if(auto *panel = window->findChild<QObject *>("settingsPanel")) panel->setProperty("open", true);
        QTimer::singleShot(3000, &app,
                           [window, screenshot]
                           {
                               window->grabWindow().save(screenshot);
                               QCoreApplication::quit();
                           });
    }

    return app.exec();
}
