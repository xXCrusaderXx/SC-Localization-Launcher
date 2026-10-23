#pragma once

#include <sodium.h>
#include <QCryptographicHash>
#include <QHash>
#include <QMutex>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "backend/utils/paths.hpp"

// Placeholder translation; real global.ini files must not be part of this repository (LEGAL.md)
inline const QByteArray TRANSLATION = "test_key=Deutscher Testtext\n";
inline const QByteArray TRANSLATION_FR = "test_key=Texte de test\n";

// Minimal HTTP server on 127.0.0.1. It runs in its own thread, so it keeps answering
// while a test blocks the main thread. "{base}" in a served file becomes the server's URL.
class TestServer : public QThread
{
   private:
    QMutex mutex;
    QHash<QByteArray, QByteArray> files;
    // "GET /de.ini 200" -> number of requests
    QHash<QByteArray, int> requestCounts;
    std::promise<quint16> portPromise;
    std::shared_future<quint16> portFuture = portPromise.get_future().share();

    void answer (QTcpSocket* socket, const QByteArray& request)
    {
        const QList<QByteArray> lines = request.split('\n');
        const QList<QByteArray> requestLine = lines.value(0).split(' ');
        const QByteArray method = requestLine.value(0);
        const QByteArray path = requestLine.value(1);
        QByteArray ifNoneMatch;
        for(const auto& line : lines)
            if(line.toLower().startsWith("if-none-match:")) ifNoneMatch = line.mid(14).trimmed();

        std::optional<QByteArray> file;
        {
            QMutexLocker lock(&mutex);
            if(files.contains(path)) file = files.value(path);
        }

        QByteArray status = "404 Not Found";
        QByteArray headers;
        QByteArray body;
        if(file)
        {
            body = QByteArray(*file).replace("{base}", url("").toUtf8());
            QByteArray etag = "\"" + QCryptographicHash::hash(body, QCryptographicHash::Sha1).toHex() + "\"";
            headers = "ETag: " + etag + "\r\n";
            status = "200 OK";
            if(ifNoneMatch == etag)
            {
                status = "304 Not Modified";
                body.clear();
            }
        }
        {
            QMutexLocker lock(&mutex);
            ++requestCounts[method + " " + path + " " + status.left(3)];
        }
        QByteArray length = QByteArray::number(body.size());
        if(method == "HEAD") body.clear();
        socket->write("HTTP/1.1 " + status + "\r\n" + headers + "Content-Length: " + length + "\r\nConnection: close\r\n\r\n" + body);
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

    // Changes a served file, e.g. to publish a new translation version
    void setFile (const QByteArray& path, const QByteArray& content)
    {
        QMutexLocker lock(&mutex);
        files[path] = content;
    }

    // e.g. requestCount("GET /de.ini 200")
    int requestCount (const QByteArray& request)
    {
        QMutexLocker lock(&mutex);
        return requestCounts.value(request);
    }
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
    // A newer launcher release; html_url points away from GitHub on purpose, the launcher must not open it
    nlohmann::json release = {{"tag_name", "v9.9.9"}, {"body", "- New feature"}, {"html_url", "https://example.org/release"}, {"prerelease", false}};
    return {{"/languages.json", QByteArray::fromStdString(languages.dump())},
            {"/releases/latest", QByteArray::fromStdString(release.dump())},
            {"/channels.json", QByteArray::fromStdString(channels.dump())},
            {"/de.ini", TRANSLATION},
            {"/fr.ini", TRANSLATION_FR}};
}

inline std::filesystem::path settingsFilePath () { return PATHS::JSON_FILES::SETTINGS_USER; }

// Every test starts without settings and without cached downloads
// The tests delete and overwrite the launcher's user folders. Started outside ctest (e.g. ./tests/GuiTest), those are the
// real folders of the person running them, so stop before touching anything that is not inside the ctest home.
inline void requireTestHome (const std::filesystem::path& path)
{
    const char* testHome = std::getenv("SC_TEST_HOME");
    std::string prefix = testHome && *testHome ? std::string(testHome) + "/" : std::string();
    if(prefix.empty() || path.string().rfind(prefix, 0) != 0)
    {
        std::cerr << "Refusing to touch " << path << ": run the tests through ctest, which gives them their own HOME.\n";
        std::abort();
    }
}

inline void resetUserDir ()
{
    for(const auto& dir : {PATHS::CONFIG_USER, PATHS::CACHE_USER, PATHS::LEGACY_ROOT_USER})
    {
        requireTestHome(dir);
        std::filesystem::remove_all(dir);
    }
}

inline void writeSettingsFile (const nlohmann::json& json, const std::filesystem::path& path = settingsFilePath())
{
    requireTestHome(path);
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

// QCOMPARE cannot take nlohmann::json with Qt 6.4: json converts implicitly to anything, including qfloat16.
// Comparing the JSON text works everywhere and prints readable values on failure.
inline std::string jsonText (const nlohmann::json& json) { return json.dump(); }

inline std::string readFile (const std::filesystem::path& path)
{
    std::ifstream stream(path);
    return std::string(std::istreambuf_iterator<char>(stream), {});
}

// Signs like `minisign -S` (prehashed "ED" format), with a fresh key per instance; never the project's real key
class TestSigningKey
{
   private:
    std::array<unsigned char, crypto_sign_PUBLICKEYBYTES> publicKey{};
    std::array<unsigned char, crypto_sign_SECRETKEYBYTES> secretKey{};
    std::array<unsigned char, 8> keyId{};

    static std::string base64 (const std::vector<unsigned char>& bytes)
    {
        std::string text(sodium_base64_ENCODED_LEN(bytes.size(), sodium_base64_VARIANT_ORIGINAL), '\0');
        sodium_bin2base64(text.data(), text.size(), bytes.data(), bytes.size(), sodium_base64_VARIANT_ORIGINAL);
        text.resize(std::strlen(text.c_str()));
        return text;
    }

   public:
    TestSigningKey ()
    {
        if(sodium_init() < 0) std::abort();
        crypto_sign_keypair(publicKey.data(), secretKey.data());
        randombytes_buf(keyId.data(), keyId.size());
    }

    // Content of a minisign .pub file
    std::string publicKeyFile () const
    {
        std::vector<unsigned char> bytes{'E', 'd'};
        bytes.insert(bytes.end(), keyId.begin(), keyId.end());
        bytes.insert(bytes.end(), publicKey.begin(), publicKey.end());
        return "untrusted comment: test key\n" + base64(bytes) + "\n";
    }

    // Content of a .minisig file for `data`
    std::string sign (const std::string& data, const std::string& trustedComment) const
    {
        std::array<unsigned char, crypto_generichash_BYTES_MAX> hash{};
        crypto_generichash(hash.data(), hash.size(), reinterpret_cast<const unsigned char*>(data.data()), data.size(), nullptr, 0);
        std::array<unsigned char, crypto_sign_BYTES> signature{};
        crypto_sign_detached(signature.data(), nullptr, hash.data(), hash.size(), secretKey.data());

        std::vector<unsigned char> line{'E', 'D'};
        line.insert(line.end(), keyId.begin(), keyId.end());
        line.insert(line.end(), signature.begin(), signature.end());

        std::vector<unsigned char> global(signature.begin(), signature.end());
        global.insert(global.end(), trustedComment.begin(), trustedComment.end());
        std::array<unsigned char, crypto_sign_BYTES> globalSignature{};
        crypto_sign_detached(globalSignature.data(), nullptr, global.data(), global.size(), secretKey.data());

        return "untrusted comment: test signature\n" + base64(line) + "\ntrusted comment: " + trustedComment + "\n" +
               base64({globalSignature.begin(), globalSignature.end()}) + "\n";
    }
};

inline std::string sha256Hex (const std::string& data)
{
    return QCryptographicHash::hash(QByteArray::fromStdString(data), QCryptographicHash::Sha256).toHex().toStdString();
}

inline const std::string APPIMAGE_ASSET = "SC-Localization-Launcher-9.9.9-x86_64.AppImage";

// Publishes launcher release v9.9.9 with an AppImage on the test server; checksums and signature default to correct ones
inline void publishAppImageRelease (TestServer& server, const TestSigningKey& key, const std::string& package,
                                    std::optional<std::string> checksums = {}, std::optional<std::string> signature = {})
{
    std::string sums = checksums.value_or(sha256Hex(package) + "  " + APPIMAGE_ASSET + "\n");
    std::string sig = signature.value_or(key.sign(sums, "sc-localization-launcher v9.9.9"));
    nlohmann::json release = {{"tag_name", "v9.9.9"},
                              {"body", "notes"},
                              {"html_url", "https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases/tag/v9.9.9"},
                              {"assets",
                               {{{"name", APPIMAGE_ASSET}, {"browser_download_url", "{base}/download/package"}},
                                {{"name", "SHA256SUMS"}, {"browser_download_url", "{base}/download/SHA256SUMS"}},
                                {{"name", "SHA256SUMS.minisig"}, {"browser_download_url", "{base}/download/SHA256SUMS.minisig"}}}}};
    server.setFile("/releases/latest", QByteArray::fromStdString(release.dump()));
    server.setFile("/download/package", QByteArray::fromStdString(package));
    server.setFile("/download/SHA256SUMS", QByteArray::fromStdString(sums));
    server.setFile("/download/SHA256SUMS.minisig", QByteArray::fromStdString(sig));
}
