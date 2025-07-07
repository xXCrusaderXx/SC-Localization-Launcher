#pragma once

#include <QWidget>
#include <filesystem>
#include <iostream>

#include "utils/paths.hpp"
#include "utils/styles.hpp"
#include "utils/utils.hpp"

class PtuEnv : public QWidget
{
   private:
    std::string selection = "undefined";
    bool isInstalled = false;

   public:
    QPushButton *buttonPtu = nullptr;
    QPushButton *buttonPtuEng = nullptr;
    QPushButton *buttonPtuDe = nullptr;

    explicit PtuEnv (QWidget *parent = nullptr, QGridLayout *grid = nullptr)
        : QWidget(parent)
    {
        buttonPtu = new QPushButton("PTU", this);
        buttonPtuEng = new QPushButton("eng", this);
        buttonPtuDe = new QPushButton("de", this);

        grid->addWidget(buttonPtu, 3, 0);
        grid->addWidget(buttonPtuEng, 3, 1);
        grid->addWidget(buttonPtuDe, 3, 2);

        connect(buttonPtu, &QPushButton::clicked, this, &PtuEnv::clicked_Ptu);
        connect(buttonPtuEng, &QPushButton::clicked, this, &PtuEnv::clicked_PtuEng);
        connect(buttonPtuDe, &QPushButton::clicked, this, &PtuEnv::clicked_PtuDe);

        buttonPtu->setStyleSheet(Style::EnvButtons::ButtonError);
        buttonPtuEng->setStyleSheet(Style::EnvButtons::ButtonDisabled);
        buttonPtuDe->setStyleSheet(Style::EnvButtons::ButtonDisabled);

        buttonPtu->setDisabled(true);
        buttonPtuEng->setDisabled(true);
        buttonPtuDe->setDisabled(true);

        updateButtons();
    }

    void clicked_Ptu () { updateFiles(); }

    void clicked_PtuEng ()
    {
        std::cout << "clicked - PTU-eng!" << std::endl;
        buttonPtuEng->setStyleSheet(Style::EnvButtons::ButtonActiv);
        buttonPtuDe->setStyleSheet(Style::EnvButtons::ButtonInactive);
        selection = "Eng";
        buttonPtuEng->setDisabled(true);
        buttonPtuDe->setDisabled(false);
    }
    void clicked_PtuDe ()
    {
        std::cout << "clicked - PTU-de!" << std::endl;
        buttonPtuEng->setStyleSheet(Style::EnvButtons::ButtonInactive);
        buttonPtuDe->setStyleSheet(Style::EnvButtons::ButtonActiv);
        selection = "De";
        buttonPtuEng->setDisabled(false);
        buttonPtuDe->setDisabled(true);
    }

    void updateButtons ()
    {
        if(utils::checkDirectoryExist(PATHS::PTU::ORIGIN_DIR) && utils::checkDirectoryExist(PATHS::PTU::TRANSLATION_DE_DIR))
        {
            isInstalled = true;
            buttonPtu->setStyleSheet(Style::EnvButtons::ButtonInfo);
            buttonPtu->setDisabled(false);
            buttonPtuDe->setDisabled(false);
            buttonPtuEng->setDisabled(false);

            clicked_PtuEng();
        }
    }

    bool backupOriginFiles ()
    {
        if(!utils::checkFileExist(PATHS::PTU::BACKUP_USER_CFG))
        {
            try
            {
                std::filesystem::create_directories(PATHS::PTU::BACKUP_DIR);
                utils::copySingleFile(PATHS::PTU::ORIGIN_USER_CFG, PATHS::PTU::BACKUP_USER_CFG);
            }
            catch(const std::filesystem::filesystem_error &e)
            {
                return false;
            }
        }

        if(!utils::checkDirectoryExist(PATHS::PTU::BACKUP_GLOBAL_INI))
        {
            try
            {
                std::filesystem::create_directories(PATHS::PTU::BACKUP_DIR);
                utils::copySingleFile(PATHS::PTU::ORIGIN_GLOBAL_INI, PATHS::PTU::BACKUP_GLOBAL_INI);
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

        std::cout << "[PTU] update selection: " << selection << std::endl;
        if(selection == "Eng")
        {
            utils::copySingleFile(PATHS::PTU::BACKUP_USER_CFG, PATHS::PTU::ORIGIN_USER_CFG);
            if(!utils::checkDirectoryExist(PATHS::PTU::ORIGIN_DIR / "data")) std::filesystem::create_directories(PATHS::PTU::ORIGIN_DIR / "data");
            utils::copySingleFile(PATHS::PTU::BACKUP_GLOBAL_INI, PATHS::PTU::ORIGIN_GLOBAL_INI);
            return true;
        }
        else if(selection == "De")
        {
            utils::copySingleFile(PATHS::PTU::TRANSLATION_DE_USER_CFG, PATHS::PTU::ORIGIN_USER_CFG);
            if(!utils::checkDirectoryExist(PATHS::PTU::ORIGIN_DIR / "data")) std::filesystem::create_directories(PATHS::PTU::ORIGIN_DIR / "data");
            utils::copySingleFile(PATHS::PTU::TRANSLATION_DE_GLOBAL_INI, PATHS::PTU::ORIGIN_GLOBAL_INI);
            return true;
        }
        else
        {
            return false;
        }
    }
};
