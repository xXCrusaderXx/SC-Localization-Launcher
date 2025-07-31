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
            LOG_DEBUG(lg) << "update() - Channel: " << channel << " translation!";
            if(data.controls.buttonEng_Selected)
            {
                // utils::copySingleFile(PATHS::LIVE::BACKUP_USER_CFG, PATHS::LIVE::ORIGIN_USER_CFG);
                // if(!utils::checkDirectoryExist(PATHS::LIVE::ORIGIN_DIR / "data"))
                //     std::filesystem::create_directories(PATHS::LIVE::ORIGIN_DIR / "data");
                // utils::copySingleFile(PATHS::LIVE::BACKUP_GLOBAL_INI, PATHS::LIVE::ORIGIN_GLOBAL_INI);

                std::ofstream file(data.paths.installPath / "user.cfg");
                if(file.is_open())
                {
                    file << "g_language = english\n";
                    file << "g_languageAudio = english\n";
                    file.close();
                }
            }
            else if(data.controls.buttonDe_Selected)
            {
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
