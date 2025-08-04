#pragma once

#include <curl/curl.h>
#include <filesystem>
#include <fstream>

#include "backend/database/DataBase.hpp"
#include "backend/utils/utils.hpp"
#include "logging/logging.h"

class UpdateHandler
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
    UpdateHandler ()
        : lg("UpdateHandler")
    {
        LOG_DEBUG(lg) << "instanziated";
    };
    ~UpdateHandler () { LOG_DEBUG(lg) << "destructed"; }

    void update (const Database::DataBase& database)
    {
        for(const auto& [channel, data] : database.channelData)
        {
            if(data.paths.installPath.empty())
            {
                LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: Path is empty, skip!";
                continue;
            }

            LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: Path: " << data.paths.installPath;

            if(data.controls.buttonEng_Selected)
            {
                LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: Eng-selected";

                if(std::filesystem::exists(data.paths.installPath / "user.cfg"))
                {
                    std::filesystem::remove(data.paths.installPath / "user.cfg");
                    LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: user.cfg gelöscht";
                }
                else
                {
                    LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: user.cfg existiert nicht";
                }

                if(std::filesystem::exists(data.paths.installPath / "data"))
                {
                    std::filesystem::remove_all(data.paths.installPath / "data");
                    LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: data/ gelöscht";
                }
                else
                {
                    LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: data/ existiert nicht";
                }
            }
            else if(data.controls.buttonDe_Selected)
            {
                LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: De-selected";
                if(!utils::checkDirectoryExist(data.paths.installPath / "data")) std::filesystem::create_directories(data.paths.installPath / "data");

                std::ofstream file(data.paths.installPath / "user.cfg");
                if(file.is_open())
                {
                    file << "g_language = german_(germany)\n";
                    file << "g_languageAudio = english\n";
                    file.close();
                }
                downloadAndWriteToFile(data.info.server1, data.paths.installPath / "data" / "global.ini");
            }
            else if(data.controls.buttonDeFull_Selected)
            {
                LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: DeVoll-selected";
                if(!utils::checkDirectoryExist(data.paths.installPath / "data")) std::filesystem::create_directories(data.paths.installPath / "data");

                std::ofstream file(data.paths.installPath / "user.cfg");
                if(file.is_open())
                {
                    file << "g_language = german_(germany)\n";
                    file << "g_languageAudio = english\n";
                    file.close();
                }

                downloadAndWriteToFile(data.info.server1, data.paths.installPath / "data" / "global.ini");
            }
            else
            {
            }
        }
    }
};
