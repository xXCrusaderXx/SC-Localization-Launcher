#pragma once

#include <curl/curl.h>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "logging/logging.h"
#include "protocol/State.hpp"

// Finds out whether a newer launcher version is published as a GitHub release. Installing it is not done here yet.
class SelfUpdateManager
{
   private:
    LoggerFramework::LogEx lg;
    const std::atomic<bool>& shuttingDown;

    inline static const std::string LATEST_RELEASE_URL = "https://api.github.com/repos/xXCrusaderXx/SC-Localization-Launcher/releases/latest";

    static std::string latestReleaseUrl ()
    {
#ifdef SC_TESTING
        // Test builds only: the tests serve the release from a local server
        if(const char* url = std::getenv("SC_RELEASES_URL")) return url;
#endif
        return LATEST_RELEASE_URL;
    }

    static size_t writeCallback (void* contents, size_t size, size_t nmemb, void* userp)
    {
        static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    static int abortOnShutdown (void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
    {
        return static_cast<const std::atomic<bool>*>(clientp)->load() ? 1 : 0;
    }

    // "v1.2.3", "1.2.3" or "1.2.3-rc1" -> {1, 2, 3}; nullopt if it is no version at all
    static std::optional<std::array<int, 3>> parseVersion (std::string text)
    {
        if(!text.empty() && (text[0] == 'v' || text[0] == 'V')) text.erase(0, 1);
        std::array<int, 3> parts{0, 0, 0};
        size_t pos = 0;
        for(int i = 0; i < 3; ++i)
        {
            size_t end = pos;
            while(end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) ++end;
            if(end == pos) return std::nullopt;
            parts[i] = std::stoi(text.substr(pos, end - pos));
            if(i < 2)
            {
                if(end >= text.size() || text[end] != '.') return std::nullopt;
                pos = end + 1;
            }
        }
        return parts;
    }

   public:
    SelfUpdateManager (const std::atomic<bool>& shuttingDown)
        : lg("SelfUpdateManager")
        , shuttingDown(shuttingDown)
    {
        LOG_DEBUG(lg) << "instanziated";
    }
    ~SelfUpdateManager () { LOG_DEBUG(lg) << "destructed"; }

    static bool isNewer (const std::string& latest, const std::string& current)
    {
        auto latestVersion = parseVersion(latest);
        auto currentVersion = parseVersion(current);
        return latestVersion && currentVersion && *latestVersion > *currentVersion;
    }

    // Blocks while asking GitHub. Never throws; problems end up in SelfUpdate::error.
    Protocol::SelfUpdate check (const std::string& currentVersion)
    {
        Protocol::SelfUpdate result;
        result.currentVersion = currentVersion;
        result.status = Protocol::SelfUpdateStatus::Error;

        CURL* curl = curl_easy_init();
        if(!curl)
        {
            result.error = "network error";
            return result;
        }

        std::string url = latestReleaseUrl();
        std::string body;
        std::string userAgent = "sc-localization-launcher/" + currentVersion;
        curl_slist* headers = curl_slist_append(nullptr, "Accept: application/vnd.github+json");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortOnShutdown);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, const_cast<std::atomic<bool>*>(&shuttingDown));

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        if(res != CURLE_OK)
        {
            LOG_ERROR(lg) << "check() - " << url << " :: " << curl_easy_strerror(res);
            result.error = "network error";
            return result;
        }
        // GitHub allows 60 unauthenticated API requests per hour and address
        if(httpCode == 403 || httpCode == 429)
        {
            LOG_ERROR(lg) << "check() - rate limited (HTTP " << httpCode << ")";
            result.error = "rate limit";
            return result;
        }
        // No release published yet
        if(httpCode == 404)
        {
            result.status = Protocol::SelfUpdateStatus::UpToDate;
            return result;
        }
        if(httpCode != 200)
        {
            LOG_ERROR(lg) << "check() - " << url << " :: HTTP " << httpCode;
            result.error = "HTTP " + std::to_string(httpCode);
            return result;
        }

        auto json = nlohmann::json::parse(body, nullptr, false);
        if(json.is_discarded() || !json.is_object() || !json.contains("tag_name") || !json.at("tag_name").is_string())
        {
            LOG_ERROR(lg) << "check() - unexpected answer from GitHub";
            result.error = "unexpected answer";
            return result;
        }

        std::string tag = json.at("tag_name").get<std::string>();
        result.latestVersion = (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) ? tag.substr(1) : tag;
        result.notes = json.value("body", "");
        result.url = json.value("html_url", "");
        // The release page is opened in the browser, so it must not point anywhere but GitHub
        if(!result.url.starts_with("https://github.com/")) result.url = "https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases/latest";
        result.status = isNewer(tag, currentVersion) ? Protocol::SelfUpdateStatus::Available : Protocol::SelfUpdateStatus::UpToDate;
        result.error.clear();
        LOG_DEBUG(lg) << "check() - current " << currentVersion << ", latest " << result.latestVersion;
        return result;
    }
};
