#pragma once

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QLocale>
#include <QObject>
#include <QPalette>
#include <QProcess>
#include <QQmlEngine>
#include <QStyleHints>
#include <QTranslator>
#include <QUrl>
#include <QVariantMap>

#include "backend/BackEnd.hpp"
#include "backend/utils/paths.hpp"
#include "logging/logging.h"

// Connects the QML GUI with the BackEnd.
// QML calls the methods below and binds to `state`, the latest snapshot of the backend.
class BackendBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString logFolder READ logFolder CONSTANT)
    Q_PROPERTY(QVariantList interfaceLanguages READ interfaceLanguages CONSTANT)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY systemDarkChanged)

   private:
    LoggerFramework::LogEx lg;
    QVariantMap currentState;
    QTranslator translator;
    // Set by followInterfaceLanguage(); without it the interface stays as it is (command line, tests)
    QQmlEngine* engine = nullptr;
    QString appliedLanguage;
    // Declared last: its thread calls back into this object
    BackEnd backEnd;

    static QVariantMap toVariant (const Protocol::State& state)
    {
        return QJsonDocument::fromJson(QByteArray::fromStdString(nlohmann::json(state).dump())).toVariant().toMap();
    }

   public:
    explicit BackendBridge (QObject* parent = nullptr)
        : QObject(parent)
        , lg("BackendBridge")
        , currentState(toVariant({}))
        , backEnd(
              [this] (const Protocol::State& state)
              {
                  // The backend publishes from its own thread, QML must only be touched from the GUI thread
                  QVariantMap variant = toVariant(state);
                  QMetaObject::invokeMethod(
                      this,
                      [this, variant]
                      {
                          currentState = variant;
                          applyInterfaceLanguage(currentState.value("interfaceLanguage").toString());
                          emit stateChanged();
                      },
                      Qt::QueuedConnection);
              },
              // Set by main() from the CMake project version
              QCoreApplication::applicationVersion().toStdString())
    {
        // Only the window checks in the background; the command line changes nothing on its own
        backEnd.execute(Protocol::StartBackgroundChecks{});
        // The desktop may switch between light and dark while the launcher runs
        QCoreApplication::instance()->installEventFilter(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &BackendBridge::systemDarkChanged);
#endif
    }

    bool eventFilter (QObject* watched, QEvent* event) override
    {
        if(watched == QCoreApplication::instance() && event->type() == QEvent::ApplicationPaletteChange) emit systemDarkChanged();
        return QObject::eventFilter(watched, event);
    }

    QVariantMap state () const { return currentState; }

    // The interface translations built into the launcher: code and name in its own language, English first
    static QVariantList interfaceLanguages ()
    {
        QStringList codes;
        for(const auto& file : QDir(":/i18n").entryList({"sc-localization-launcher_*.qm"}, QDir::Files, QDir::Name))
            codes << file.mid(QString("sc-localization-launcher_").size()).chopped(3);
        codes.removeAll("en");
        codes.prepend("en");
        QVariantList languages;
        for(const auto& code : codes)
        {
            // Qt names English after the US ("American English")
            QString name = code == "en" ? QString("English") : QLocale(code).nativeLanguageName();
            if(!name.isEmpty()) name[0] = name[0].toUpper();
            languages << QVariantMap{{"code", code}, {"name", name}};
        }
        return languages;
    }

    // From now on the interface follows state.interfaceLanguage, at once and without a restart
    void followInterfaceLanguage (QQmlEngine* qmlEngine)
    {
        engine = qmlEngine;
        appliedLanguage = "-";
        applyInterfaceLanguage(currentState.value("interfaceLanguage").toString());
    }

    void applyInterfaceLanguage (const QString& language)
    {
        if(!engine || language == appliedLanguage) return;
        appliedLanguage = language;
        QCoreApplication::removeTranslator(&translator);
        // The interface is written in English; without a translation for the language it stays English
        bool loaded = language.isEmpty() ? translator.load(QLocale(), "sc-localization-launcher", "_", ":/i18n")
                                         : translator.load("sc-localization-launcher_" + language, ":/i18n");
        if(loaded) QCoreApplication::installTranslator(&translator);
        engine->retranslate();
    }

    // Whether the desktop uses a dark colour scheme
    static bool systemDark ()
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        if(auto scheme = QGuiApplication::styleHints()->colorScheme(); scheme != Qt::ColorScheme::Unknown) return scheme == Qt::ColorScheme::Dark;
#endif
        // Qt 6.4 knows no colour scheme; the platform theme's window colour tells it
        return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
    }

    // "dark", "light", or empty to follow the system
    Q_INVOKABLE void setTheme (const QString& theme) { backEnd.execute(Protocol::SetTheme{.theme = theme.toStdString()}); }

    Q_INVOKABLE void finishSetup () { backEnd.execute(Protocol::FinishSetup{}); }

    // Empty = the system language
    Q_INVOKABLE void setInterfaceLanguage (const QString& language)
    {
        backEnd.execute(Protocol::SetInterfaceLanguage{.language = language.toStdString()});
    }

    // language is a code from state.languages, the backend rejects unknown codes
    Q_INVOKABLE void setLanguage (const QString& channel, const QString& language)
    {
        backEnd.execute(Protocol::SetLanguage{.channel = channel.toStdString(), .language = language.toStdString()});
    }

    Q_INVOKABLE void setInstallPath (const QString& channel, const QUrl& folder)
    {
        backEnd.execute(Protocol::SetInstallPath{.channel = channel.toStdString(), .path = folder.toLocalFile().toStdString()});
    }

    Q_INVOKABLE void setRsiLauncherPath (const QUrl& folder)
    {
        backEnd.execute(Protocol::SetRsiLauncherPath{.path = folder.toLocalFile().toStdString()});
    }

    Q_INVOKABLE void setOption (const QString& option, bool value)
    {
        backEnd.execute(Protocol::SetOption{.option = option.toStdString(), .value = value});
    }

    Q_INVOKABLE void startUpdate () { backEnd.execute(Protocol::StartUpdate{}); }
    // From the tray menu: only the translations, the game is not started
    Q_INVOKABLE void updateTranslations () { backEnd.execute(Protocol::StartUpdate{.channels = {}, .allowGameStart = false}); }
    Q_INVOKABLE void startGame () { backEnd.execute(Protocol::StartGame{}); }

    // An empty url removes the channel's own source; the backend checks url and folder again
    Q_INVOKABLE void setCustomSource (const QString& channel, const QString& url, const QString& folder)
    {
        backEnd.execute(Protocol::SetCustomSource{
            .channel = channel.toStdString(), .url = url.trimmed().toStdString(), .folder = folder.trimmed().toStdString()});
    }
    Q_INVOKABLE void repairChannel (const QString& channel) { backEnd.execute(Protocol::RepairChannel{.channel = channel.toStdString()}); }
    Q_INVOKABLE void autoSearch () { backEnd.execute(Protocol::AutoSearch{}); }
    Q_INVOKABLE void checkForUpdate () { backEnd.execute(Protocol::CheckForUpdate{}); }
    Q_INVOKABLE void installUpdate () { backEnd.execute(Protocol::InstallUpdate{}); }
    Q_INVOKABLE void loadWhatsNew () { backEnd.execute(Protocol::LoadWhatsNew{}); }
    Q_INVOKABLE void dismissWhatsNew () { backEnd.execute(Protocol::DismissWhatsNew{}); }
    Q_INVOKABLE void scanCaches () { backEnd.execute(Protocol::ScanCaches{}); }
    Q_INVOKABLE void runDiagnostics () { backEnd.execute(Protocol::RunDiagnostics{}); }
    Q_INVOKABLE void saveReport () { backEnd.execute(Protocol::SaveReport{}); }
    Q_INVOKABLE void loadBackups () { backEnd.execute(Protocol::LoadBackups{}); }
    Q_INVOKABLE void setGameSettings (const QString& channel, int maxFps, int maxIdleFps, const QString& customLines, const QString& graphicsApi)
    {
        backEnd.execute(Protocol::SetGameSettings{
            .channel = channel.toStdString(),
            .settings = {
                .maxFps = maxFps, .maxIdleFps = maxIdleFps, .customLines = customLines.toStdString(), .graphicsApi = graphicsApi.toStdString()}});
    }
    // programs: objects with command, enabled and stopWithGame, as in State.programs
    Q_INVOKABLE void setPrograms (const QVariantList& programs)
    {
        Protocol::SetPrograms command;
        for(const auto& entry : programs)
        {
            auto program = entry.toMap();
            command.programs.push_back({.command = program.value("command").toString().toStdString(),
                                        .enabled = program.value("enabled", true).toBool(),
                                        .stopWithGame = program.value("stopWithGame").toBool()});
        }
        backEnd.execute(command);
    }
    Q_INVOKABLE void createBackup (const QString& channel) { backEnd.execute(Protocol::CreateBackup{.channel = channel.toStdString()}); }
    Q_INVOKABLE void restoreBackup (const QString& channel, const QString& backup)
    {
        backEnd.execute(Protocol::RestoreBackup{.channel = channel.toStdString(), .backup = backup.toStdString()});
    }
    // Shows the saved report in the file manager
    Q_INVOKABLE void openReportFolder (const QString& report) const
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(report).absolutePath()));
    }
    Q_INVOKABLE void clearCaches (const QStringList& paths)
    {
        Protocol::ClearCaches command;
        for(const auto& path : paths) command.paths.push_back(path.toStdString());
        backEnd.execute(command);
    }

    // Starts the updated launcher and quits this one; the new one waits until this one released its lock
    Q_INVOKABLE void restartLauncher ()
    {
        QString program = qEnvironmentVariableIsSet("APPIMAGE") ? qEnvironmentVariable("APPIMAGE") : QCoreApplication::applicationFilePath();
        QStringList arguments{"--after-update"};
        // A process started inside this sandbox would still run the old version, so the host starts the new one
        if(QFile::exists("/.flatpak-info"))
        {
            program = "flatpak-spawn";
            arguments = QStringList{"--host", "flatpak", "run", SelfUpdateManager::FLATPAK_APP_ID} + arguments;
        }
        if(!QProcess::startDetached(program, arguments))
        {
            LOG_ERROR(lg) << "restartLauncher() - cannot start " << program.toStdString();
            return;
        }
        QGuiApplication::quit();
    }

    Q_INVOKABLE QUrl fromLocalFile (const QString& path) const { return QUrl::fromLocalFile(path); }

    // Where CustomLogger writes; shown in the settings and opened from error messages
    QString logFolder () const { return QString::fromStdString((PATHS::LOGPATH_USER / "logs").string()); }
    Q_INVOKABLE void openLogFolder () const { QDesktopServices::openUrl(QUrl::fromLocalFile(logFolder())); }

   signals:
    void stateChanged ();
    void systemDarkChanged ();
};
