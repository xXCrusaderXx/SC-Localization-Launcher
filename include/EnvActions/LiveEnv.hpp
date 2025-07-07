#pragma once

#include <QWidget>
#include <filesystem>
#include <iostream>

#include "utils/paths.hpp"
#include "utils/styles.hpp"
#include "utils/utils.hpp"

class LiveEnv : public QWidget
{
   private:
    std::string selection = "undefined";
    bool isInstalled = false;

   public:
    QPushButton *buttonLive = nullptr;
    QPushButton *buttonLiveEng = nullptr;
    QPushButton *buttonLiveDe = nullptr;
    QPushButton *buttonLiveDeVoll = nullptr;

    explicit LiveEnv (QWidget *parent = nullptr, QGridLayout *grid = nullptr)
        : QWidget(parent)
    {
        buttonLive = new QPushButton("LIVE", this);
        buttonLiveEng = new QPushButton("eng", this);
        buttonLiveDe = new QPushButton("de", this);
        buttonLiveDeVoll = new QPushButton("de-voll", this);

        grid->addWidget(buttonLive, 2, 0);
        grid->addWidget(buttonLiveEng, 2, 1);
        grid->addWidget(buttonLiveDe, 2, 2);
        grid->addWidget(buttonLiveDeVoll, 2, 3);

        connect(buttonLive, &QPushButton::clicked, this, &LiveEnv::clicked_Live);
        connect(buttonLiveEng, &QPushButton::clicked, this, &LiveEnv::clicked_LiveEng);
        connect(buttonLiveDe, &QPushButton::clicked, this, &LiveEnv::clicked_LiveDe);
        connect(buttonLiveDeVoll, &QPushButton::clicked, this, &LiveEnv::clicked_LiveDeFull);

        buttonLive->setStyleSheet(Style::EnvButtons::ButtonError);
        buttonLiveEng->setStyleSheet(Style::EnvButtons::ButtonDisabled);
        buttonLiveDe->setStyleSheet(Style::EnvButtons::ButtonDisabled);
        buttonLiveDeVoll->setStyleSheet(Style::EnvButtons::ButtonDisabled);

        buttonLive->setDisabled(true);
        buttonLiveEng->setDisabled(true);
        buttonLiveDe->setDisabled(true);
        buttonLiveDeVoll->setDisabled(true);

        updateButtons();
    }

    void clicked_Live () { updateFiles(); }

    void clicked_LiveEng ()
    {
        std::cout << "clicked - LIVE-eng!" << std::endl;
        buttonLiveEng->setStyleSheet(Style::EnvButtons::ButtonActiv);
        buttonLiveDe->setStyleSheet(Style::EnvButtons::ButtonInactive);
        buttonLiveDeVoll->setStyleSheet(Style::EnvButtons::ButtonInactive);
        selection = "Eng";
        buttonLiveEng->setDisabled(true);
        buttonLiveDe->setDisabled(false);
        buttonLiveDeVoll->setDisabled(false);
    }
    void clicked_LiveDe ()
    {
        std::cout << "clicked - LIVE-de!" << std::endl;
        buttonLiveEng->setStyleSheet(Style::EnvButtons::ButtonInactive);
        buttonLiveDe->setStyleSheet(Style::EnvButtons::ButtonActiv);
        buttonLiveDeVoll->setStyleSheet(Style::EnvButtons::ButtonInactive);
        selection = "De";
        buttonLiveEng->setDisabled(false);
        buttonLiveDe->setDisabled(true);
        buttonLiveDeVoll->setDisabled(false);
    }
    void clicked_LiveDeFull ()
    {
        std::cout << "clicked - LIVE-de-voll!" << std::endl;
        buttonLiveEng->setStyleSheet(Style::EnvButtons::ButtonInactive);
        buttonLiveDe->setStyleSheet(Style::EnvButtons::ButtonInactive);
        buttonLiveDeVoll->setStyleSheet(Style::EnvButtons::ButtonActiv);
        selection = "DeVoll";
        buttonLiveEng->setDisabled(false);
        buttonLiveDe->setDisabled(false);
        buttonLiveDeVoll->setDisabled(true);
    }

    void updateButtons ()
    {
        if(utils::checkDirectoryExist(PATHS::LIVE::ORIGIN_DIR) && utils::checkDirectoryExist(PATHS::LIVE::TRANSLATION_DE_DIR))
        {
            isInstalled = true;
            buttonLive->setStyleSheet(Style::EnvButtons::ButtonInfo);
            buttonLive->setDisabled(false);
            buttonLiveEng->setDisabled(false);
            buttonLiveDe->setDisabled(false);

            if(utils::checkDirectoryExist(PATHS::LIVE::TRANSLATION_DEVOLL_DIR))
            {
                buttonLiveDeVoll->setDisabled(false);
            }

            clicked_LiveEng();
        }
    }

    bool backupOriginFiles ()
    {
        if(!utils::checkFileExist(PATHS::LIVE::BACKUP_USER_CFG))
        {
            try
            {
                std::filesystem::create_directories(PATHS::LIVE::BACKUP_DIR);
                utils::copySingleFile(PATHS::LIVE::ORIGIN_USER_CFG, PATHS::LIVE::BACKUP_USER_CFG);
            }
            catch(const std::filesystem::filesystem_error &e)
            {
                return false;
            }
        }

        if(!utils::checkDirectoryExist(PATHS::LIVE::BACKUP_GLOBAL_INI))
        {
            try
            {
                std::filesystem::create_directories(PATHS::LIVE::BACKUP_DIR);
                utils::copySingleFile(PATHS::LIVE::ORIGIN_GLOBAL_INI, PATHS::LIVE::BACKUP_GLOBAL_INI);
            }
            catch(const std::filesystem::filesystem_error &e)
            {
                return false;
            }
        }
        return true;
    }

    bool updateFiles ()
    {
        if(!isInstalled) return false;

        std::cout << "[LIVE] update selection: " << selection << std::endl;
        backupOriginFiles();
        if(selection == "Eng")
        {
            utils::copySingleFile(PATHS::LIVE::BACKUP_USER_CFG, PATHS::LIVE::ORIGIN_USER_CFG);
            if(!utils::checkDirectoryExist(PATHS::LIVE::ORIGIN_DIR / "data")) std::filesystem::create_directories(PATHS::LIVE::ORIGIN_DIR / "data");
            utils::copySingleFile(PATHS::LIVE::BACKUP_GLOBAL_INI, PATHS::LIVE::ORIGIN_GLOBAL_INI);
            return true;
        }
        else if(selection == "De")
        {
            utils::copySingleFile(PATHS::LIVE::TRANSLATION_DE_USER_CFG, PATHS::LIVE::ORIGIN_USER_CFG);
            if(!utils::checkDirectoryExist(PATHS::LIVE::ORIGIN_DIR / "data")) std::filesystem::create_directories(PATHS::LIVE::ORIGIN_DIR / "data");
            utils::copySingleFile(PATHS::LIVE::TRANSLATION_DE_GLOBAL_INI, PATHS::LIVE::ORIGIN_GLOBAL_INI);
            return true;
        }
        else if(selection == "DeVoll")
        {
            utils::copySingleFile(PATHS::LIVE::TRANSLATION_DEVOLL_USER_CFG, PATHS::LIVE::ORIGIN_USER_CFG);
            if(!utils::checkDirectoryExist(PATHS::LIVE::ORIGIN_DIR / "data")) std::filesystem::create_directories(PATHS::LIVE::ORIGIN_DIR / "data");
            utils::copySingleFile(PATHS::LIVE::TRANSLATION_DEVOLL_GLOBAL_INI, PATHS::LIVE::ORIGIN_GLOBAL_INI);
            return true;
        }
        else
        {
            return false;
        }
    }
};
