#pragma once

#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <filesystem>
#include <fstream>
#include <future>
#include <nlohmann/json.hpp>
#include <string>

#include "backend/utils/paths.hpp"

// Placeholder translation; real global.ini files must not be part of this repository (LEGAL.md)
inline const QByteArray TRANSLATION = "test_key=Deutscher Testtext\n";

// Minimal HTTP server on 127.0.0.1. It runs in its own thread, so it keeps answering
// while a test blocks the main thread. "{base}" in a served file becomes the server's URL.
class TestServer : public QThread
{
   private:
    QHash<QByteArray, QByteArray> files;
    std::promise<quint16> portPromise;
    std::shared_future<quint16> portFuture = portPromise.get_future().share();

    void answer (QTcpSocket* socket, const QByteArray& request)
    {
        QByteArray path = request.split(' ').value(1);
        bool found = files.contains(path);
        QByteArray body = found ? QByteArray(files.value(path)).replace("{base}", url("").toUtf8()) : QByteArray();
        socket->write("HTTP/1.1 " + QByteArray(found ? "200 OK" : "404 Not Found") + "\r\nContent-Length: " + QByteArray::number(body.size()) +
                      "\r\nConnection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
    }

    void run () override
    {
        QTcpServer server;
        QObject::connect(&server, &QTcpServer::newConnection,
                         [&]
                         {
                             while(QTcpSocket* socket = server.nextPendingConnection())
                             {
                                 QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                                 QObject::connect(socket, &QTcpSocket::readyRead, socket,
                                                  [this, socket]
                                                  {
                                                      // Requests may arrive in pieces, answer once the header is complete
                                                      QByteArray request = socket->property("request").toByteArray() + socket->readAll();
                                                      socket->setProperty("request", request);
                                                      if(request.contains("\r\n\r\n")) answer(socket, request);
                                                  });
                             }
                         });
        server.listen(QHostAddress::LocalHost);
        portPromise.set_value(server.serverPort());
        exec();
    }

   public:
    explicit TestServer (QHash<QByteArray, QByteArray> files)
        : files(std::move(files))
    {
        start();
    }

    ~TestServer () override
    {
        quit();
        wait();
    }

    QString url (const QString& path) { return QString("http://127.0.0.1:%1%2").arg(portFuture.get()).arg(path); }
};

// LIVE: German only through the fallback URL, German full is listed but cannot be downloaded.
// PTU: switched off on the server. TECH-PREV: the server's name for TECH-PREVIEW. EPTU: only in "backup".
inline QHash<QByteArray, QByteArray> fixtureFiles ()
{
    nlohmann::json channels = {{"channels",
                                {{{"active", 1},
                                  {"description", "LIVE"},
                                  {"server1", "{base}/missing.ini"},
                                  {"server1_fallback", "{base}/de.ini"},
                                  {"server3", "{base}/missing.ini"},
                                  {"server3_fallback", "{base}/missing-too.ini"}},
                                 {{"active", 0}, {"description", "PTU"}, {"server1", "{base}/de.ini"}},
                                 {{"active", 1}, {"description", "TECH-PREV"}, {"server1", "{base}/de.ini"}}}},
                               {"backup", {{"EPTU", {{"active", 0}, {"server1", "{base}/de.ini"}}}}}};
    return {{"/channels.json", QByteArray::fromStdString(channels.dump())}, {"/de.ini", TRANSLATION}};
}

inline std::filesystem::path settingsFilePath () { return PATHS::JSON_FILES::SETTINGS_USER; }

// Every test starts without settings and without a cached channels.json
inline void resetUserDir () { std::filesystem::remove_all(PATHS::ROOT_USER); }

inline void writeSettingsFile (const nlohmann::json& json)
{
    std::filesystem::create_directories(settingsFilePath().parent_path());
    std::ofstream(settingsFilePath()) << json.dump(4);
}

inline nlohmann::json readSettingsFile ()
{
    std::ifstream stream(settingsFilePath());
    return nlohmann::json::parse(stream, nullptr, false);
}

// settings.json as written before the protocol refactor
inline nlohmann::json legacySettings (const std::string& livePath, const std::string& liveTranslation, bool launchGame)
{
    return {{"channels", {{"LIVE", {{"installPath", livePath}, {"selectedTrans", liveTranslation}}}}},
            {"settings",
             {{"LaunchScAfterTranslation", launchGame},
              {"autoNewTranslation", false},
              {"autoTranslationAtStart", false},
              {"minimizeScdAfterUpdate", false},
              {"rsiLauncherInstallPath", "/nonexistent/RSI Launcher"},
              {"showUpdateStatus", false},
              {"startScdWithSystemStart", false}}}};
}
