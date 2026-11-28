#pragma once

#include <curl/curl.h>
#include <strings.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <sstream>
#include <string>

#include "backend/jsonParser/LanguageCatalogParser.hpp"
#include "backend/utils/GitHubRepository.hpp"
#include "backend/utils/paths.hpp"
#include "backend/utils/platform.hpp"
#include "logging/logging.h"
#include "protocol/State.hpp"

// What a translation project published on GitHub: its releases, or, if it has none, the commits that changed the channel's
// file. Asked anonymously through the public REST API, which allows 60 requests per hour and address: every answer is cached
// on disk, used for a while without asking, then revalidated with its ETag (GitHub does not count a "not modified"), and
// after a rate limit nothing is asked until GitHub's reset time. Release and commit texts are plain text for the UI.
// Blocks the calling thread, so it runs in a background job.
class TranslationChanges
{
   public:
    // At most this many releases or commits, newest first
    static constexpr int MAX_ENTRIES = 10;
    static constexpr size_t MAX_TITLE_LENGTH = 200;
    static constexpr size_t MAX_TEXT_LENGTH = 3000;

    // maxAge: how long a cached answer is used without asking GitHub
    explicit TranslationChanges (const std::atomic<bool>& shuttingDown, std::filesystem::path cacheDir = PATHS::CACHE_USER / "github",
                                 std::chrono::seconds maxAge = std::chrono::minutes(10))
        : lg("TranslationChanges")
        , shuttingDown(shuttingDown)
        , cacheDir(std::move(cacheDir))
        , maxAge(maxAge)
    {
    }

    // file: the channel's translation file in the repository, for the commits; installedAt: when the installed copy was
    // downloaded, seconds since the epoch; what was published later is marked pending. Fills kind, entries, error, loaded.
    Protocol::TranslationChanges load (const std::string& repository, const std::optional<GitHubRepository::File>& file,
                                       std::optional<std::int64_t> installedAt)
    {
        Protocol::TranslationChanges result;
        result.repository = repository;
        if(!GitHubRepository::isValid(repository))
        {
            result.error = "unexpected answer";
            return result;
        }

        auto releases = get("repos/" + repository + "/releases?per_page=" + std::to_string(MAX_ENTRIES));
        if(!releases.body)
        {
            result.error = releases.error;
            return result;
        }
        auto json = nlohmann::json::parse(*releases.body, nullptr, false);
        if(!json.is_array())
        {
            LOG_ERROR(lg) << "load() - unexpected releases answer for " << repository;
            result.error = "unexpected answer";
            return result;
        }
        for(const auto& release : json)
        {
            if(!release.is_object() || release.value("draft", false)) continue;
            std::string tag = text(release, "tag_name");
            std::string name = text(release, "name");
            result.entries.push_back({plainText(name.empty() ? tag : name, MAX_TITLE_LENGTH, true), plainText(text(release, "body"), MAX_TEXT_LENGTH),
                                      parseTime(text(release, "published_at")), pageOf(text(release, "html_url"), repository), false});
        }
        if(!result.entries.empty())
            result.kind = "releases";
        else if(file)
        {
            auto commits = get("repos/" + repository + "/commits?path=" + escape(file->path) + "&sha=" + escape(file->branch) +
                               "&per_page=" + std::to_string(MAX_ENTRIES));
            if(!commits.body)
            {
                result.error = commits.error;
                return result;
            }
            json = nlohmann::json::parse(*commits.body, nullptr, false);
            if(!json.is_array())
            {
                LOG_ERROR(lg) << "load() - unexpected commits answer for " << repository;
                result.error = "unexpected answer";
                return result;
            }
            for(const auto& commit : json)
            {
                if(!commit.is_object() || !commit.contains("commit") || !commit["commit"].is_object()) continue;
                const auto& details = commit["commit"];
                std::string message = text(details, "message");
                size_t lineEnd = message.find('\n');
                std::string title = message.substr(0, lineEnd);
                std::string body = lineEnd == std::string::npos ? "" : message.substr(lineEnd + 1);
                std::string date = details.contains("committer") && details["committer"].is_object() ? text(details["committer"], "date") : "";
                result.entries.push_back({plainText(title, MAX_TITLE_LENGTH, true), plainText(body, MAX_TEXT_LENGTH), parseTime(date),
                                          pageOf(text(commit, "html_url"), repository), false});
            }
            if(!result.entries.empty()) result.kind = "commits";
        }

        std::stable_sort(result.entries.begin(), result.entries.end(), [] (const auto& a, const auto& b) { return a.time > b.time; });
        if(result.entries.size() > MAX_ENTRIES) result.entries.resize(MAX_ENTRIES);
        if(installedAt)
            for(auto& entry : result.entries) entry.pending = entry.time > *installedAt;
        result.loaded = true;
        LOG_INFO(lg) << "load() - " << repository << ": " << result.entries.size() << " " << (result.kind.empty() ? "entries" : result.kind);
        return result;
    }

    // Text from the internet for a plain text label: no control characters, at most one empty line in a row, cut to
    // maxLength bytes without splitting a UTF-8 character. oneLine also joins the lines.
    static std::string plainText (const std::string& text, size_t maxLength, bool oneLine = false)
    {
        std::string result;
        for(char c : text)
        {
            auto byte = static_cast<unsigned char>(c);
            if((c == '\n' && oneLine) || c == '\t')
                result += ' ';
            else if(c == '\n' || (byte >= 0x20 && byte != 0x7f))
                result += c;
        }
        // Spaces at the end of every line, more than one empty line, spaces and lines at both ends of the text
        static const std::regex trailingSpaces(" +\n");
        static const std::regex emptyLines("\n{3,}");
        result = std::regex_replace(std::regex_replace(result, trailingSpaces, "\n"), emptyLines, "\n\n");
        result.erase(0, result.find_first_not_of(" \n"));
        result.erase(result.find_last_not_of(" \n") + 1);
        if(result.size() > maxLength)
        {
            result.resize(maxLength);
            while(!result.empty() && (static_cast<unsigned char>(result.back()) & 0xC0) == 0x80) result.pop_back();
            if(!result.empty() && (static_cast<unsigned char>(result.back()) & 0x80)) result.pop_back();
            result += "…";
        }
        return result;
    }

    // "2026-11-22T10:47:15Z" as GitHub writes it -> seconds since the epoch; 0 for anything else
    static std::int64_t parseTime (const std::string& text)
    {
        static const std::regex pattern(R"((\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})Z)");
        std::smatch match;
        if(!std::regex_match(text, match, pattern)) return 0;
        auto number = [&] (int i) { return std::stoll(match[i].str()); };
        // Days since 1970-01-01 of a date in the proleptic Gregorian calendar (H. Hinnant's days_from_civil)
        std::int64_t year = number(1), month = number(2), day = number(3);
        if(month < 1 || month > 12 || day < 1 || day > 31) return 0;
        year -= month <= 2;
        std::int64_t era = (year >= 0 ? year : year - 399) / 400;
        std::int64_t yearOfEra = year - era * 400;
        std::int64_t dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        std::int64_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
        std::int64_t days = era * 146097 + dayOfEra - 719468;
        return days * 86400 + number(4) * 3600 + number(5) * 60 + number(6);
    }

   private:
    inline static const std::string API_URL = "https://api.github.com/";
    // Without a personal detail; GitHub rejects API requests without any User-Agent
    inline static const std::string USER_AGENT = "SC-Localization-Launcher (https://github.com/xXCrusaderXx/SC-Localization-Launcher)";

    LoggerFramework::LogEx lg;
    const std::atomic<bool>& shuttingDown;
    std::filesystem::path cacheDir;
    std::chrono::seconds maxAge;
    // Set by a rate limit: until then (seconds since the epoch) GitHub is not asked
    std::int64_t blockedUntil = 0;

    static std::string apiUrl ()
    {
#ifdef SC_TESTING
        // Test builds only: the tests serve the API from a local server
        if(const char* url = std::getenv("SC_GITHUB_API_URL")) return url;
#endif
        return API_URL;
    }

    static std::string text (const nlohmann::json& object, const char* key)
    {
        auto it = object.find(key);
        return it != object.end() && it->is_string() ? it->get<std::string>() : std::string();
    }

    // Only pages of the project's own repository are offered as a link
    static std::string pageOf (const std::string& url, const std::string& repository)
    {
        std::string prefix = GitHubRepository::PAGE_PREFIX + repository + "/";
        if(url.size() <= prefix.size() || !GitHubRepository::same(url.substr(0, prefix.size()), prefix)) return {};
        bool plain = std::all_of(url.begin(), url.end(), [] (unsigned char c) { return c > 0x20 && c < 0x7f && c != '"' && c != '\\'; });
        return plain ? url : std::string();
    }

    static std::string escape (const std::string& text)
    {
        char* escaped = curl_easy_escape(nullptr, text.c_str(), static_cast<int>(text.size()));
        std::string result = escaped ? escaped : "";
        curl_free(escaped);
        return result;
    }

    struct Response
    {
        long status = 0;
        std::string body;
        std::string etag;
        std::string rateLimitRemaining;
        std::int64_t rateLimitReset = 0;
    };

    struct Answer
    {
        // Empty when there is neither an answer nor a cached copy
        std::optional<std::string> body;
        // "network error", "rate limit" or "unexpected answer"
        std::string error;
    };

    // path: of the request below the API address, e.g. "repos/<owner>/<name>/releases?per_page=10"
    std::filesystem::path cacheFile (const std::string& path) const
    {
        std::ostringstream name;
        name << std::hex << std::hash<std::string>{}(path) << ".json";
        return cacheDir / name.str();
    }

    std::optional<nlohmann::json> readCache (const std::string& path) const
    {
        std::ifstream in(cacheFile(path));
        if(!in) return std::nullopt;
        auto json = nlohmann::json::parse(in, nullptr, false);
        if(!json.is_object() || json.value("path", "") != path || !json.contains("body") || !json["body"].is_string()) return std::nullopt;
        return json;
    }

    // Written next to the old copy and moved over it, so an interrupted write never leaves a broken cache
    void writeCache (const std::string& path, const nlohmann::json& json)
    {
        std::error_code ec;
        std::filesystem::create_directories(cacheDir, ec);
        auto file = cacheFile(path);
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

    Answer get (const std::string& path)
    {
        std::string url = apiUrl() + path;
        auto cached = readCache(path);
        std::int64_t now = std::time(nullptr);
        auto fromCache = [&] (const std::string& error)
        {
            if(cached) LOG_DEBUG(lg) << "get() - " << error << ", using the cached copy of " << url;
            return cached ? Answer{(*cached)["body"].get<std::string>(), ""} : Answer{std::nullopt, error};
        };
        if(cached && now - cached->value("fetched", std::int64_t{0}) < maxAge.count()) return fromCache("fresh");
        if(now < blockedUntil) return fromCache("rate limit");

        auto response = request(url, cached ? cached->value("etag", "") : "");
        if(response.status == 304 && cached)
        {
            (*cached)["fetched"] = now;
            writeCache(path, *cached);
            return {(*cached)["body"].get<std::string>(), ""};
        }
        if(response.status == 200)
        {
            // Not valid JSON: the parse in load() reports it, nothing is cached
            if(nlohmann::json::accept(response.body))
                writeCache(path, {{"path", path}, {"etag", response.etag}, {"fetched", now}, {"body", response.body}});
            return {response.body, ""};
        }
        if(response.status == 429 || (response.status == 403 && response.rateLimitRemaining == "0"))
        {
            // GitHub names its reset time; without one, or a strange one, an hour
            blockedUntil = response.rateLimitReset > now && response.rateLimitReset - now <= 3600 ? response.rateLimitReset : now + 3600;
            LOG_ERROR(lg) << "get() - rate limited (HTTP " << response.status << ") until " << blockedUntil;
            return fromCache("rate limit");
        }
        if(response.status == 0) return fromCache("network error");
        LOG_ERROR(lg) << "get() - " << url << " :: HTTP " << response.status;
        return fromCache("unexpected answer");
    }

    static size_t writeCallback (void* contents, size_t size, size_t nmemb, void* userp)
    {
        auto* body = static_cast<std::string*>(userp);
        // Ten releases are a few kilobytes; a much larger answer is not what was asked for
        if(body->size() + size * nmemb > 4 * 1024 * 1024) return 0;
        body->append(static_cast<char*>(contents), size * nmemb);
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
        if(strncasecmp(line.c_str(), "etag:", 5) == 0) response->etag = value(5);
        if(strncasecmp(line.c_str(), "x-ratelimit-remaining:", 22) == 0) response->rateLimitRemaining = value(22);
        if(strncasecmp(line.c_str(), "x-ratelimit-reset:", 18) == 0) response->rateLimitReset = std::strtoll(value(18).c_str(), nullptr, 10);
        return size * nitems;
    }

    static int abortOnShutdown (void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
    {
        return static_cast<const std::atomic<bool>*>(clientp)->load() ? 1 : 0;
    }

    Response request (const std::string& url, const std::string& etag)
    {
        Response response;
        if(!LanguageCatalogParser::isAllowedUrl(url))
        {
            LOG_ERROR(lg) << "request() - not an HTTPS address: " << url;
            return response;
        }
        CURL* curl = curl_easy_init();
        Platform::useSystemCertificates(curl);
        if(!curl) return response;
        curl_slist* headers = curl_slist_append(nullptr, "Accept: application/vnd.github+json");
        if(!etag.empty()) headers = curl_slist_append(headers, ("If-None-Match: " + etag).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT.c_str());
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
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortOnShutdown);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &shuttingDown);

        CURLcode code = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        if(code != CURLE_OK)
        {
            LOG_ERROR(lg) << "request() - " << url << " :: " << curl_easy_strerror(code);
            response.status = 0;
        }
        return response;
    }
};
