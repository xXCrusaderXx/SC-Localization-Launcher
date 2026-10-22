#pragma once

#include <QJsonDocument>
#include <QObject>
#include <QUrl>
#include <QVariantMap>

#include "backend/BackEnd.hpp"
#include "logging/logging.h"

// Connects the QML GUI with the BackEnd.
// QML calls the methods below and binds to `state`, the latest snapshot of the backend.
class BackendBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)

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
              })
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

    Q_INVOKABLE QUrl fromLocalFile (const QString& path) const { return QUrl::fromLocalFile(path); }

   signals:
    void stateChanged ();
};
