#pragma once

#include <QWidget>
#include <filesystem>
#include <iostream>

#include "utils/paths.hpp"
#include "utils/styles.hpp"
#include "utils/utils.hpp"

class HotfixEnv : public QWidget
{
   private:
    std::string selection;
    bool isInstalled = false;

   public:
    QPushButton *buttonHotfix;
    QPushButton *buttonHotfixEng;

    explicit HotfixEnv (QWidget *parent = nullptr, QGridLayout *grid = nullptr)
        : QWidget(parent)
    {
        buttonHotfix = new QPushButton("HOTFIX", this);
        buttonHotfixEng = new QPushButton("eng", this);

        grid->addWidget(buttonHotfix, 5, 0);
        grid->addWidget(buttonHotfixEng, 5, 1);

        connect(buttonHotfix, &QPushButton::clicked, this, &HotfixEnv::clicked_Hotfix);
        connect(buttonHotfixEng, &QPushButton::clicked, this, &HotfixEnv::clicked_HotfixEng);

        buttonHotfix->setDisabled(true);
        buttonHotfixEng->setDisabled(true);

        buttonHotfix->setStyleSheet(Style::EnvButtons::ButtonError);
        buttonHotfixEng->setStyleSheet(Style::EnvButtons::ButtonDisabled);
    }

    void clicked_Hotfix () { std::cout << "clicked - HOTFIX!" << std::endl; }
    void clicked_HotfixEng ()
    {
        std::cout << "clicked - HOTFIX-eng!" << std::endl;
        buttonHotfixEng->setStyleSheet(Style::EnvButtons::ButtonActiv);
        selection = "Eng";
    }

    bool updateFiles ()
    {
        if(!isInstalled) return false;

        std::cout << "[HOTFIX] update selection: " << selection << std::endl;
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
