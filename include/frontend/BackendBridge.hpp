#pragma once

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QObject>
#include <QProcess>
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

   private:
    LoggerFramework::LogEx lg;
    QVariantMap currentState;
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
                          emit stateChanged();
                      },
                      Qt::QueuedConnection);
              },
              // Set by main() from the CMake project version
              QCoreApplication::applicationVersion().toStdString())
    {
        // Only the window checks in the background; the command line changes nothing on its own
        backEnd.execute(Protocol::StartBackgroundChecks{});
    }

    QVariantMap state () const { return currentState; }

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
};
