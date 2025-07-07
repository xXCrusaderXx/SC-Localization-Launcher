#pragma once

#include <QWidget>
#include <filesystem>
#include <iostream>

#include "utils/paths.hpp"
#include "utils/styles.hpp"
#include "utils/utils.hpp"

class EptuEnv : public QWidget
{
   private:
    std::string selection;
    bool isInstalled = false;

   public:
    QPushButton *buttonEptu = nullptr;
    QPushButton *buttonEptuEng = nullptr;

    explicit EptuEnv (QWidget *parent = nullptr, QGridLayout *grid = nullptr)
        : QWidget(parent)
    {
        buttonEptu = new QPushButton("EPTU", this);
        buttonEptuEng = new QPushButton("eng", this);

        grid->addWidget(buttonEptu, 4, 0);
        grid->addWidget(buttonEptuEng, 4, 1);

        connect(buttonEptu, &QPushButton::clicked, this, &EptuEnv::clicked_Eptu);
        connect(buttonEptuEng, &QPushButton::clicked, this, &EptuEnv::clicked_EptuEng);

        buttonEptu->setStyleSheet(Style::EnvButtons::ButtonError);
        buttonEptuEng->setStyleSheet(Style::EnvButtons::ButtonDisabled);

        buttonEptu->setDisabled(true);
        buttonEptuEng->setDisabled(true);

        updateButtons();
    }

    void clicked_Eptu () { updateFiles(); }
    void clicked_EptuEng ()
    {
        std::cout << "clicked - EPTU-eng!" << std::endl;
        buttonEptuEng->setStyleSheet(Style::EnvButtons::ButtonActiv);
        selection = "Eng";
    }

    void updateButtons ()
    {
        if(utils::checkDirectoryExist(PATHS::EPTU::ORIGIN_DIR) && utils::checkDirectoryExist(PATHS::EPTU::TRANSLATION_DE_DIR))
        {
            isInstalled = true;
            buttonEptu->setStyleSheet(Style::EnvButtons::ButtonInfo);
            buttonEptu->setDisabled(false);
            buttonEptuEng->setDisabled(false);

            clicked_EptuEng();
        }
    }

    bool updateFiles ()
    {
        if(!isInstalled) return false;

        std::cout << "[EPTU] update selection: " << selection << std::endl;
        if(selection == "Eng")
        {
            return true;
        }
        else
        {
            return false;
        }
    }
};
