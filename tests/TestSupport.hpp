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
inline const QByteArray TRANSLATION_FR = "test_key=Texte de test\n";

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

inline nlohmann::json testCredit ()
{
    return {{"project", "Test-INI"},
            {"authors", "Test community"},
            {"url", "https://example.org/test-ini"},
            {"license", "CC BY-NC-SA 4.0"},
            {"licenseUrl", "https://creativecommons.org/licenses/by-nc-sa/4.0/"}};
}

// de: fixed URLs, LIVE only through the fallback URL; TECH-PREV is the server name for TECH-PREVIEW.
// de-full: listed for LIVE, but neither URL can be downloaded.
// fr: a channel list in the sc-deutsch-launcher.de format; PTU is switched off there, EPTU is only in "backup".
// The last four entries are invalid (unsafe folder, plain HTTP, duplicate code, no credit) and must be ignored.
inline QHash<QByteArray, QByteArray> fixtureFiles ()
{
    nlohmann::json languages = {
        {"version", 1},
        {"languages",
         {{{"code", "de"},
           {"name", "Deutsch"},
           {"folder", "german_(germany)"},
           {"source", {{"channels", {{"LIVE", {"{base}/missing.ini", "{base}/de.ini"}}, {"TECH-PREV", {"{base}/de.ini"}}}}}}},
          {{"code", "de-full"},
           {"name", "Deutsch (vollständig)"},
           {"folder", "german_(germany)"},
           {"source", {{"channels", {{"LIVE", {"{base}/missing.ini", "{base}/missing-too.ini"}}}}}}},
          {{"code", "fr"},
           {"name", "Français"},
           {"folder", "french_(france)"},
           {"source", {{"channelList", "{base}/channels.json"}, {"server", "server1"}}}},
          {{"code", "evil"}, {"name", "Evil"}, {"folder", "../../evil"}, {"source", {{"channels", {{"LIVE", {"{base}/de.ini"}}}}}}},
          {{"code", "plain"}, {"name", "Plain HTTP"}, {"folder", "plain"}, {"source", {{"channels", {{"LIVE", {"http://example.com/global.ini"}}}}}}},
          {{"code", "de"}, {"name", "Duplicate"}, {"folder", "german_(germany)"}, {"source", {{"channels", {{"PTU", {"{base}/de.ini"}}}}}}},
          {{"code", "nocredit"}, {"name", "No credit"}, {"folder", "nocredit"}, {"source", {{"channels", {{"LIVE", {"{base}/de.ini"}}}}}}}}}};
    for(auto& language : languages["languages"])
        if(language["code"] != "nocredit") language["credit"] = testCredit();
    nlohmann::json channels = {{"channels",
                                {{{"active", 1}, {"description", "LIVE"}, {"server1", "{base}/fr.ini"}},
                                 {{"active", 0}, {"description", "PTU"}, {"server1", "{base}/fr.ini"}}}},
                               {"backup", {{"EPTU", {{"active", 1}, {"server1", "{base}/missing.ini"}, {"server1_fallback", "{base}/fr.ini"}}}}}};
    return {{"/languages.json", QByteArray::fromStdString(languages.dump())},
            {"/channels.json", QByteArray::fromStdString(channels.dump())},
            {"/de.ini", TRANSLATION},
            {"/fr.ini", TRANSLATION_FR}};
}

inline std::filesystem::path settingsFilePath () { return PATHS::JSON_FILES::SETTINGS_USER; }

// Every test starts without settings and without cached downloads
inline void resetUserDir ()
{
    for(const auto& dir : {PATHS::CONFIG_USER, PATHS::CACHE_USER, PATHS::LEGACY_ROOT_USER}) std::filesystem::remove_all(dir);
}

inline void writeSettingsFile (const nlohmann::json& json, const std::filesystem::path& path = settingsFilePath())
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << json.dump(4);
}

inline nlohmann::json readSettingsFile ()
{
    std::ifstream stream(settingsFilePath());
    return nlohmann::json::parse(stream, nullptr, false);
}

// settings.json of the former SC Deutsch Launcher, written before the protocol refactor
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

inline std::string readFile (const std::filesystem::path& path)
{
    std::ifstream stream(path);
    return std::string(std::istreambuf_iterator<char>(stream), {});
}
