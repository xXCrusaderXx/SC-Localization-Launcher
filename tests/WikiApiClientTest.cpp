#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTest>

#include "TestSupport.hpp"
#include "backend/managers/WikiApiClient.hpp"
#include "logging/logger/custom_logger/custom_logger.h"

// The Wiki API client against a local server that answers like the API: pages, Last-Modified, 429 and outages
class WikiApiClientTest : public QObject
{
    Q_OBJECT

   private:
    TestServer server{{}};
    std::atomic<bool> shuttingDown{false};
    std::unique_ptr<QTemporaryDir> cache;

    // The parameters in the order the client sends them (sorted by name)
    static QByteArray page (int number, const QByteArray& filter = {})
    {
        return "/api/v2/items?" + filter + "page%5Bnumber%5D=" + QByteArray::number(number) + "&page%5Bsize%5D=200";
    }

    static QByteArray body (const nlohmann::json& entries, int lastPage)
    {
        return QByteArray::fromStdString(nlohmann::json{{"data", entries}, {"meta", {{"last_page", lastPage}}}}.dump());
    }

    WikiApiClient client () { return WikiApiClient(shuttingDown, cache->path().toStdString(), std::chrono::milliseconds(0)); }

    // Only the fields #21 needs, like the launcher will keep them
    static nlohmann::json reduce (const nlohmann::json& item) { return {{"uuid", item["uuid"]}, {"size", item["size"]}}; }

   private slots:
    void initTestCase () { LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>(false)); }

    void init ()
    {
        cache = std::make_unique<QTemporaryDir>();
        qputenv("SC_WIKI_API_URL", server.url("/api/v2/").toUtf8());
        server.setFile(page(1), body({{{"uuid", "a"}, {"size", 1}, {"name", "Alpha"}}, {{"uuid", "b"}, {"size", 2}, {"name", "Beta"}}}, 2));
        server.setFile(page(2), body({{{"uuid", "c"}, {"size", 3}, {"name", "Gamma"}}}, 2));
        server.setLastModified(page(1), "Sat, 24 Oct 2026 18:49:09 GMT");
    }

    void fetchesAllPagesAndKeepsOnlyWhatIsNeeded ()
    {
        auto result = client().fetchAll("items", {}, reduce);
        QVERIFY(result.ok);
        QVERIFY(!result.fromCache);
        QCOMPARE(jsonText(result.entries),
                 jsonText(nlohmann::json{{{"uuid", "a"}, {"size", 1}}, {{"uuid", "b"}, {"size", 2}}, {{"uuid", "c"}, {"size", 3}}}));
    }

    void sendsFiltersAsQueryParameters ()
    {
        server.setFile(page(1, "filter%5Btype%5D=WeaponPersonal&"), body({{{"uuid", "w"}, {"size", 1}}}, 1));
        auto result = client().fetchAll("items", {{"filter[type]", "WeaponPersonal"}}, reduce);
        QVERIFY(result.ok);
        QCOMPARE(jsonText(result.entries), jsonText(nlohmann::json{{{"uuid", "w"}, {"size", 1}}}));
    }

    void usesAFreshCacheWithoutAsking ()
    {
        QVERIFY(client().fetchAll("items", {}, reduce).ok);
        int requests = server.requestCount("GET " + page(1) + " 200");

        auto again = client().fetchAll("items", {}, reduce);
        QVERIFY(again.ok);
        QVERIFY(again.fromCache);
        QCOMPARE(again.entries.size(), size_t(3));
        QCOMPARE(server.requestCount("GET " + page(1) + " 200"), requests);
    }

    void asksWhetherAnOlderCacheIsStillCurrent ()
    {
        QVERIFY(client().fetchAll("items", {}, reduce).ok);
        int pageTwo = server.requestCount("GET " + page(2) + " 200");

        // Older than allowed: the first page is asked If-Modified-Since, the server says unchanged
        auto again = client().fetchAll("items", {}, reduce, std::chrono::seconds(0));
        QVERIFY(again.ok);
        QVERIFY(again.fromCache);
        QCOMPARE(server.requestCount("GET " + page(1) + " 304"), 1);
        QCOMPARE(server.requestCount("GET " + page(2) + " 200"), pageTwo);
        QCOMPARE(again.entries.size(), size_t(3));
    }

    void ignoresACacheWrittenWithAnotherFormat ()
    {
        QVERIFY(client().fetchAll("items", {}, reduce).ok);
        int pageOne = server.requestCount("GET " + page(1) + " 200");

        // Fresh, but kept by an older reducer: loaded again in full instead of revalidated
        auto again = client().fetchAll("items", {}, reduce, std::chrono::hours(24), "with-names");
        QVERIFY(again.ok);
        QVERIFY(!again.fromCache);
        QCOMPARE(server.requestCount("GET " + page(1) + " 200"), pageOne + 1);
        QVERIFY(client().fetchAll("items", {}, reduce, std::chrono::hours(24), "with-names").fromCache);
    }

    void loadsAgainWhenTheServerHasNewerData ()
    {
        QVERIFY(client().fetchAll("items", {}, reduce).ok);
        server.setLastModified(page(1), "Sun, 25 Oct 2026 08:00:00 GMT");
        server.setFile(page(2), body({{{"uuid", "c"}, {"size", 4}}}, 2));

        auto again = client().fetchAll("items", {}, reduce, std::chrono::seconds(0));
        QVERIFY(again.ok);
        QVERIFY(!again.fromCache);
        QCOMPARE(jsonText(again.entries[2]), jsonText(nlohmann::json{{"uuid", "c"}, {"size", 4}}));
    }

    void waitsAsLongAsTheServerAsks ()
    {
        server.failNext(page(1), 1, "429 Too Many Requests", "1");
        QElapsedTimer timer;
        timer.start();
        auto result = client().fetchAll("items", {}, reduce);
        QVERIFY(result.ok);
        QVERIFY(!result.fromCache);
        QCOMPARE(server.requestCount("GET " + page(1) + " 429"), 1);
        QVERIFY(timer.elapsed() >= 1000);
    }

    void usesTheCacheWhenTheServerFails ()
    {
        QVERIFY(client().fetchAll("items", {}, reduce).ok);
        server.failNext(page(1), 3, "503 Service Unavailable", "0");

        auto again = client().fetchAll("items", {}, reduce, std::chrono::seconds(0));
        QVERIFY(again.ok);
        QVERIFY(again.fromCache);
        QCOMPARE(again.error, std::string("the Wiki API answered HTTP 503"));
        QCOMPARE(again.entries.size(), size_t(3));
    }

    void failsWithoutCacheWhenTheServerCannotBeReached ()
    {
        // Nothing listens on port 9 of this machine
        qputenv("SC_WIKI_API_URL", "http://127.0.0.1:9/api/v2/");
        auto result = client().fetchAll("items", {}, reduce);
        QVERIFY(!result.ok);
        QVERIFY(result.entries.empty());
        QVERIFY(result.error.starts_with("cannot reach the Wiki API"));
    }

    void neverUsesPlainHttp ()
    {
        qputenv("SC_WIKI_API_URL", "http://example.org/api/v2/");
        auto result = client().fetchAll("items", {}, reduce);
        QVERIFY(!result.ok);
        QCOMPARE(result.error, std::string("not an HTTPS address"));
    }

    void keepsTheOldCacheWhenAPageIsBroken ()
    {
        QVERIFY(client().fetchAll("items", {}, reduce).ok);
        server.setLastModified(page(1), "Sun, 25 Oct 2026 08:00:00 GMT");
        server.setFile(page(2), "not json");

        auto again = client().fetchAll("items", {}, reduce, std::chrono::seconds(0));
        QVERIFY(again.ok);
        QVERIFY(again.fromCache);
        QCOMPARE(again.error, std::string("unexpected answer"));
        QCOMPARE(jsonText(again.entries[2]), jsonText(nlohmann::json{{"uuid", "c"}, {"size", 3}}));
    }
};

QTEST_GUILESS_MAIN(WikiApiClientTest)
#include "WikiApiClientTest.moc"
