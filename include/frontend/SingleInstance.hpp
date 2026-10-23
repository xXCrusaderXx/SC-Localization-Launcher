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

    static QString runtimeDir ()
    {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        return dir.isEmpty() ? QDir::tempPath() : dir;
    }

    static QString lockPath (const QString& name) { return runtimeDir() + "/" + name + ".lock"; }

    // Next to the lock instead of Qt's default in /tmp, which is private to each Flatpak sandbox:
    // a second start there would never reach the running launcher
    static QString socketPath (const QString& name) { return runtimeDir() + "/" + name + ".socket"; }

    void listen ()
    {
        server = std::make_unique<QLocalServer>();
        server->setSocketOptions(QLocalServer::UserAccessOption);
        // A socket file left behind by a crashed launcher would block listen()
        QLocalServer::removeServer(socketPath(name));
        if(!server->listen(socketPath(name)))
            LOG_ERROR(lg) << "cannot listen on " << socketPath(name).toStdString() << ": " << server->errorString().toStdString();
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
        // Nothing logged for a second start, it would leave a log file behind for every click on the menu entry
        if(primary) listen();
    }

    bool isPrimary () const { return primary; }

    // Called by a second launcher: asks the running one to show its window
    bool activatePrimary ()
    {
        QLocalSocket socket;
        socket.connectToServer(socketPath(name));
        bool connected = socket.waitForConnected(1000);
        if(!connected) LOG_ERROR(lg) << "cannot reach the running launcher: " << socket.errorString().toStdString();
        socket.disconnectFromServer();
        return connected;
    }

    // Called when activatePrimary() failed: the lock belongs to a launcher that is gone but whose lock
    // QLockFile cannot judge, e.g. one from the native build seen inside the Flatpak (other PID namespace)
    bool takeOver ()
    {
        LOG_INFO(lg) << "taking over the lock of a launcher that does not answer";
        lockFile.removeStaleLockFile();
        primary = lockFile.tryLock(0);
        if(primary)
            listen();
        else
            LOG_ERROR(lg) << "cannot take over the lock " << lockPath(name).toStdString();
        return primary;
    }

   signals:
    void activationRequested ();
};
