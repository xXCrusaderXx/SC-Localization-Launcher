#pragma once

#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QObject>
#include <QStandardPaths>
#include <memory>

#include "logging/logging.h"

// Keeps a second launcher from starting: two backends would write the same settings.json and
// download into the same game folders at once. A second start asks the running one to show its window.
class SingleInstance : public QObject
{
    Q_OBJECT

   private:
    LoggerFramework::LogEx lg;
    QString name;
    QLockFile lockFile;
    std::unique_ptr<QLocalServer> server;
    bool primary = false;

    static QString lockPath (const QString& name)
    {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if(dir.isEmpty()) dir = QDir::tempPath();
        return dir + "/" + name + ".lock";
    }

   public:
    // name must be unique per user, e.g. the app ID. waitMs: how long to wait for a running launcher to end,
    // e.g. after an update restart
    explicit SingleInstance (const QString& name, int waitMs = 100, QObject* parent = nullptr)
        : QObject(parent)
        , lg("SingleInstance")
        , name(name)
        , lockFile(lockPath(name))
    {
        // A lock of a crashed launcher is taken over, QLockFile checks whether its process still runs
        lockFile.setStaleLockTime(0);
        primary = lockFile.tryLock(waitMs);
        if(!primary)
        {
            LOG_DEBUG(lg) << "another launcher is running";
            return;
        }

        server = std::make_unique<QLocalServer>();
        server->setSocketOptions(QLocalServer::UserAccessOption);
        // A socket file left behind by a crashed launcher would block listen()
        QLocalServer::removeServer(name);
        if(!server->listen(name)) LOG_ERROR(lg) << "cannot listen on " << name.toStdString() << ": " << server->errorString().toStdString();
        connect(server.get(), &QLocalServer::newConnection, this,
                [this]
                {
                    while(QLocalSocket* socket = server->nextPendingConnection())
                    {
                        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
                        socket->disconnectFromServer();
                        emit activationRequested();
                    }
                });
    }

    bool isPrimary () const { return primary; }

    // Called by a second launcher: asks the running one to show its window
    bool activatePrimary ()
    {
        QLocalSocket socket;
        socket.connectToServer(name);
        bool connected = socket.waitForConnected(1000);
        if(!connected) LOG_ERROR(lg) << "cannot reach the running launcher: " << socket.errorString().toStdString();
        socket.disconnectFromServer();
        return connected;
    }

   signals:
    void activationRequested ();
};
