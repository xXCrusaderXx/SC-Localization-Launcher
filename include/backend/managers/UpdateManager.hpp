#pragma once

#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <optional>

#include "backend/database/DataBase.hpp"
#include "backend/utils/utils.hpp"
#include "logging/logging.h"
#include "protocol/Protocol.hpp"
#include "protocol/UpdateButtonPayload.hpp"

class UpdateManager
{
   private:
    LoggerFramework::LogEx lg;
    static size_t writeToFileCallback (void* contents, size_t size, size_t nmemb, void* userp)
    {
        std::ofstream* outFile = static_cast<std::ofstream*>(userp);
        size_t totalSize = size * nmemb;
        outFile->write(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    bool downloadAndWriteToFile (const std::string& url, const std::filesystem::path& outputFile)
    {
        CURL* curl = curl_easy_init();
        if(!curl) return false;

        std::ofstream file(outputFile, std::ios::binary);
        if(!file.is_open())
        {
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFileCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // Redirects erlauben

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        file.close();
        return res == CURLE_OK;
    }

   public:
    UpdateManager ()
        : lg("UpdateManager")
    {
        LOG_DEBUG(lg) << "instanziated";
    };
    ~UpdateManager () { LOG_DEBUG(lg) << "destructed"; }

    std::vector<std::optional<Protocol::Response>> processRequest (const Database::BackendData& backendData, const Protocol::Request& msg)
    {
        LOG_DEBUG(lg) << "processRequest() - Start";
        {
            for(const auto& [channel, data] : backendData.channelData)
            {
                if(data.installPath.empty())
                {
                    LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: Path is empty, skip!";
                    continue;
                }

                LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: Path: " << data.installPath;

                if(data.buttonEng.selected)
                {
                    LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: Eng-selected";

                    if(std::filesystem::exists(data.installPath / "user.cfg"))
                    {
                        std::filesystem::remove(data.installPath / "user.cfg");
                        LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: user.cfg gelöscht";
                    }
                    else
                    {
                        LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: user.cfg existiert nicht";
                    }

                    if(std::filesystem::exists(data.installPath / "data"))
                    {
                        std::filesystem::remove_all(data.installPath / "data");
                        LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: data/ gelöscht";
                    }
                    else
                    {
                        LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: data/ existiert nicht";
                    }
                }
                else if(data.buttonDe.selected)
                {
                    LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: De-selected";
                    if(!utils::checkDirectoryExist(data.installPath / "data" / "Localization" / "german_(germany)"))
                        std::filesystem::create_directories(data.installPath / "data" / "Localization" / "german_(germany)");

                    std::ofstream file(data.installPath / "user.cfg");
                    if(file.is_open())
                    {
                        file << "g_language = german_(germany)\n";
                        file << "g_languageAudio = english\n";
                        file.close();
                    }
                    downloadAndWriteToFile(data.info.server1, data.installPath / "data" / "Localization" / "german_(germany)" / "global.ini");
                }
                else if(data.buttonDeFull.selected)
                {
                    LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: DeVoll-selected";
                    if(!utils::checkDirectoryExist(data.installPath / "data" / "Localization" / "german_(germany)"))
                        std::filesystem::create_directories(data.installPath / "data" / "Localization" / "german_(germany)");

                    std::ofstream file(data.installPath / "user.cfg");
                    if(file.is_open())
                    {
                        file << "g_language = german_(germany)\n";
                        file << "g_languageAudio = english\n";
                        file.close();
                    }

                    downloadAndWriteToFile(data.info.server1, data.installPath / "data" / "Localization" / "german_(germany)" / "global.ini");
                }
                else
                {
                }
            }
        }
        return std::vector<std::optional<Protocol::Response>>{};
    }
};
