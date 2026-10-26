#pragma once

#include <curl/curl.h>
#include <strings.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#include "backend/jsonParser/LanguageCatalogParser.hpp"
#include "backend/utils/paths.hpp"
#include "logging/logging.h"

// Game data (items, blueprints, missions) from the Star Citizen Wiki API, the only source the launcher uses for it.
// Wherever this data is shown, credit "Star Citizen Wiki API" (https://api.star-citizen.wiki).
//
// Every request is cached. Fresh entries are used without asking the server; older ones are revalidated with
// If-Modified-Since, and when the server cannot be reached the last cached copy is used. Pages are fetched one
// after another with a short pause, and 429/503 answers are retried after the time the server asks for.
// Blocks the calling thread, so it runs in a background job.
class WikiApiClient
{
   public:
    // e.g. {{"filter[type]", "WeaponPersonal"}}
    using Query = std::map<std::string, std::string>;
    // Keeps only what the caller needs of one entry, so the cache stays small (an item has about 40 fields)
    using Reducer = std::function<nlohmann::json(const nlohmann::json&)>;

    struct Result
    {
        nlohmann::json entries = nlohmann::json::array();
        // entries can be used: from the server or from the cache
        bool ok = false;
        bool fromCache = false;
        // Why the server could not be used, also when the cache stepped in
        std::string error;
    };

    // pageDelay: pause between two pages, to go easy on the server
    explicit WikiApiClient (const std::atomic<bool>& shuttingDown, std::filesystem::path cacheDir = PATHS::CACHE_USER / "wiki",
                            std::chrono::milliseconds pageDelay = std::chrono::milliseconds(500))
        : lg("WikiApiClient")
        , shuttingDown(shuttingDown)
        , cacheDir(std::move(cacheDir))
        , pageDelay(pageDelay)
    {
    }

    // All entries of an endpoint such as "items"; maxAge: how long a cached copy is used without asking the server.
    // format names what reduce keeps: a cached copy written with another format is not used, also not as fallback.
    Result fetchAll (const std::string& endpoint, const Query& query = {}, const Reducer& reduce = {},
                     std::chrono::seconds maxAge = std::chrono::hours(24), const std::string& format = "")
    {
        Result result;
        auto file = cacheFile(endpoint, query);
        auto cached = readCache(file, format);
        auto now = std::time(nullptr);
        if(cached && now - cached->value("fetched", std::int64_t{0}) < maxAge.count())
        {
            result.entries = (*cached)["entries"];
            result.ok = result.fromCache = true;
            return result;
        }

        nlohmann::json entries = nlohmann::json::array();
        std::string lastModified;
        for(int page = 1, lastPage = 1; page <= lastPage; ++page)
        {
            if(page > 1 && !wait(pageDelay)) return fallback(cached, "cancelled");
            Query paged = query;
            paged["page[size]"] = std::to_string(PAGE_SIZE);
            paged["page[number]"] = std::to_string(page);
            // Only the first page is asked whether anything changed; the server dates the whole list
            auto response = getWithRetries(url(endpoint, paged), page == 1 && cached ? cached->value("lastModified", "") : "");
            if(page == 1 && response.status == 304 && cached)
            {
                LOG_DEBUG(lg) << "fetchAll() - " << endpoint << " unchanged";
                (*cached)["fetched"] = now;
                writeCache(file, *cached);
                result.entries = (*cached)["entries"];
                result.ok = result.fromCache = true;
                return result;
            }
            if(response.status != 200) return fallback(cached, response.error);

            auto json = nlohmann::json::parse(response.body, nullptr, false);
            if(json.is_discarded() || !json.contains("data") || !json["data"].is_array()) return fallback(cached, "unexpected answer");
            for(const auto& entry : json["data"]) entries.push_back(reduce ? reduce(entry) : entry);
            lastPage = json.value(nlohmann::json::json_pointer("/meta/last_page"), 1);
            if(page == 1) lastModified = response.lastModified;
        }

        LOG_INFO(lg) << "fetchAll() - " << endpoint << ": " << entries.size() << " entries";
        writeCache(file, {{"fetched", now}, {"lastModified", lastModified}, {"format", format}, {"entries", entries}});
        result.entries = std::move(entries);
        result.ok = true;
        return result;
    }

   private:
    inline static const std::string BASE_URL = "https://api.star-citizen.wiki/api/v2/";
    static constexpr int PAGE_SIZE = 200;
    static constexpr int ATTEMPTS = 3;

    LoggerFramework::LogEx lg;
    const std::atomic<bool>& shuttingDown;
    std::filesystem::path cacheDir;
    std::chrono::milliseconds pageDelay;

    struct Response
    {
        long status = 0;
        std::string body;
        std::string lastModified;
        // Seconds the server asks to wait (Retry-After)
        std::optional<long> retryAfter;
        std::string error;
    };

    static std::string baseUrl ()
    {
#ifdef SC_TESTING
        // Test builds only: the tests serve the API from a local server
        if(const char* url = std::getenv("SC_WIKI_API_URL")) return url;
#endif
        return BASE_URL;
    }

    // Parameters in a fixed order, so a request always has the same URL and cache file
    static std::string url (const std::string& endpoint, const Query& query)
    {
        std::string url = baseUrl() + endpoint;
        char separator = '?';
        for(const auto& [name, value] : query)
        {
            url += separator + escape(name) + "=" + escape(value);
            separator = '&';
        }
        return url;
    }

    static std::string escape (const std::string& text)
    {
        char* escaped = curl_easy_escape(nullptr, text.c_str(), static_cast<int>(text.size()));
        std::string result = escaped ? escaped : "";
        curl_free(escaped);
        return result;
    }

    std::filesystem::path cacheFile (const std::string& endpoint, const Query& query) const
    {
        std::ostringstream name;
        name << endpoint << "-" << std::hex << std::hash<std::string>{}(url(endpoint, query)) << ".json";
        return cacheDir / name.str();
    }

    static std::optional<nlohmann::json> readCache (const std::filesystem::path& file, const std::string& format)
    {
        std::ifstream in(file);
        if(!in) return std::nullopt;
        auto json = nlohmann::json::parse(in, nullptr, false);
        if(json.is_discarded() || !json.contains("entries") || !json["entries"].is_array()) return std::nullopt;
        if(json.value("format", "") != format) return std::nullopt;
        return json;
    }

    // Written next to the old copy and moved over it, so an interrupted write never leaves a broken cache
    void writeCache (const std::filesystem::path& file, const nlohmann::json& json)
    {
        std::error_code ec;
        std::filesystem::create_directories(cacheDir, ec);
        auto temporary = file;
        temporary += ".download";
        {
            std::ofstream out(temporary);
            out << json.dump();
            if(!out)
            {
                LOG_ERROR(lg) << "writeCache() - cannot write " << temporary;
                return;
            }
        }
        std::filesystem::rename(temporary, file, ec);
        if(ec) LOG_ERROR(lg) << "writeCache() - cannot replace " << file << ": " << ec.message();
    }

    Result fallback (const std::optional<nlohmann::json>& cached, const std::string& error)
    {
        Result result;
        result.error = error;
        if(cached)
        {
            LOG_INFO(lg) << "fetchAll() - " << error << ", using the cached copy";
            result.entries = (*cached)["entries"];
            result.ok = result.fromCache = true;
        }
        else
            LOG_ERROR(lg) << "fetchAll() - " << error << ", nothing cached";
        return result;
    }

    // Sleeps in small steps, so closing the launcher is not held up; false when it is closing
    bool wait (std::chrono::milliseconds duration) const
    {
        auto end = std::chrono::steady_clock::now() + duration;
        while(!shuttingDown && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return !shuttingDown;
    }

    Response getWithRetries (const std::string& url, const std::string& ifModifiedSince)
    {
        Response response;
        for(int attempt = 1; attempt <= ATTEMPTS; ++attempt)
        {
            response = get(url, ifModifiedSince);
            bool busy = response.status == 429 || response.status == 503;
            bool unreachable = response.status == 0;
            if((!busy && !unreachable) || attempt == ATTEMPTS) break;
            // As long as the server asks, but never longer than a minute; otherwise 1 s, 2 s
            auto delay = std::chrono::seconds(std::min(response.retryAfter.value_or(1L << (attempt - 1)), 60L));
            LOG_INFO(lg) << "get() - " << response.error << ", trying again in " << delay.count() << " s";
            if(!wait(delay)) break;
        }
        return response;
    }

    static size_t writeCallback (void* contents, size_t size, size_t nmemb, void* userp)
    {
        static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    static size_t headerCallback (char* buffer, size_t size, size_t nitems, void* userdata)
    {
        auto* response = static_cast<Response*>(userdata);
        std::string line(buffer, size * nitems);
        auto value = [&] (size_t length)
        {
            auto start = line.find_first_not_of(" \t", length);
            auto end = line.find_last_not_of(" \t\r\n");
            return start == std::string::npos || end < start ? std::string() : line.substr(start, end - start + 1);
        };
        if(strncasecmp(line.c_str(), "last-modified:", 14) == 0) response->lastModified = value(14);
        if(strncasecmp(line.c_str(), "retry-after:", 12) == 0)
        {
            // Seconds only; a date is treated as "not given"
            char* end = nullptr;
            std::string text = value(12);
            long seconds = std::strtol(text.c_str(), &end, 10);
            if(end && *end == '\0' && !text.empty()) response->retryAfter = seconds;
        }
        return size * nitems;
    }

    static int abortOnShutdown (void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
    {
        return static_cast<const std::atomic<bool>*>(clientp)->load() ? 1 : 0;
    }

    Response get (const std::string& url, const std::string& ifModifiedSince)
    {
        Response response;
        if(!LanguageCatalogParser::isAllowedUrl(url))
        {
            response.error = "not an HTTPS address";
            return response;
        }
        CURL* curl = curl_easy_init();
        if(!curl)
        {
            response.error = "curl not available";
            return response;
        }
        curl_slist* headers = curl_slist_append(nullptr, "Accept: application/json");
        if(!ifModifiedSince.empty()) headers = curl_slist_append(headers, ("If-Modified-Since: " + ifModifiedSince).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "SC-Localization-Launcher (https://github.com/xXCrusaderXx/SC-Localization-Launcher)");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        // A redirect must not leave HTTPS; the AppImage is built against curl 7.81, before the string option existed
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
        curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortOnShutdown);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &shuttingDown);

        CURLcode code = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if(code != CURLE_OK)
        {
            response.status = 0;
            response.error = std::string("cannot reach the Wiki API: ") + curl_easy_strerror(code);
        }
        else if(response.status != 200 && response.status != 304)
            response.error = "the Wiki API answered HTTP " + std::to_string(response.status);
        return response;
    }
};
