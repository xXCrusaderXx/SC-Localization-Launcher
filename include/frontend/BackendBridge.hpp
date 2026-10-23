#pragma once

#include <QCoreApplication>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QObject>
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
    Q_INVOKABLE void autoSearch () { backEnd.execute(Protocol::AutoSearch{}); }
    Q_INVOKABLE void checkForUpdate () { backEnd.execute(Protocol::CheckForUpdate{}); }

    Q_INVOKABLE QUrl fromLocalFile (const QString& path) const { return QUrl::fromLocalFile(path); }

    // Where CustomLogger writes; shown in the settings and opened from error messages
    QString logFolder () const { return QString::fromStdString((PATHS::LOGPATH_USER / "logs").string()); }
    Q_INVOKABLE void openLogFolder () const { QDesktopServices::openUrl(QUrl::fromLocalFile(logFolder())); }

   signals:
    void stateChanged ();
};
