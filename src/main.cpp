#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QTranslator>

#include "frontend/BackendBridge.hpp"
#include "frontend/SingleInstance.hpp"
#include "logging/logger/custom_logger/custom_logger.h"
#include "logging/logging.h"

int main (int argc, char *argv[])
{
    LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>());

    LoggerFramework::LogEx lg("main");
    LOG_DEBUG(lg) << "Start SC Localization Launcher " << SC_VERSION;

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
    if(!instance.isPrimary()) return instance.activatePrimary() ? 0 : 1;

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
