#pragma once

#include <QWidget>
#include <filesystem>
#include <iostream>

#include "utils/paths.hpp"
#include "utils/styles.hpp"
#include "utils/utils.hpp"

class TechPreviewEnv : public QWidget
{
   private:
    std::string selection;
    bool isInstalled = false;

   public:
    QPushButton *buttonTechPreview;
    QPushButton *buttonTechPreviewEng;
    QPushButton *buttonTechPreviewDe;
    QPushButton *buttonTechPreviewDeVoll;

    explicit TechPreviewEnv (QWidget *parent = nullptr, QGridLayout *grid = nullptr)
        : QWidget(parent)
    {
        buttonTechPreview = new QPushButton("TECH-PREVIEW", this);
        buttonTechPreviewEng = new QPushButton("eng", this);
        buttonTechPreviewDe = new QPushButton("de", this);
        buttonTechPreviewDeVoll = new QPushButton("de-voll", this);

        grid->addWidget(buttonTechPreview, 6, 0);
        grid->addWidget(buttonTechPreviewEng, 6, 1);
        grid->addWidget(buttonTechPreviewDe, 6, 2);
        grid->addWidget(buttonTechPreviewDeVoll, 6, 3);

        connect(buttonTechPreview, &QPushButton::clicked, this, &TechPreviewEnv::clicked_TechPreview);
        connect(buttonTechPreviewEng, &QPushButton::clicked, this, &TechPreviewEnv::clicked_TechPreviewEng);
        connect(buttonTechPreviewDe, &QPushButton::clicked, this, &TechPreviewEnv::clicked_TechPreviewDe);
        connect(buttonTechPreviewDeVoll, &QPushButton::clicked, this, &TechPreviewEnv::clicked_TechPreviewDeFull);

        buttonTechPreview->setStyleSheet(Style::EnvButtons::ButtonError);
        buttonTechPreviewEng->setStyleSheet(Style::EnvButtons::ButtonDisabled);
        buttonTechPreviewDe->setStyleSheet(Style::EnvButtons::ButtonDisabled);
        buttonTechPreviewDeVoll->setStyleSheet(Style::EnvButtons::ButtonDisabled);

        buttonTechPreview->setDisabled(true);
        buttonTechPreviewEng->setDisabled(true);
        buttonTechPreviewDe->setDisabled(true);
        buttonTechPreviewDeVoll->setDisabled(true);
    }

    void clicked_TechPreview () { std::cout << "clicked - TECH-PREVIEW!" << std::endl; }
    void clicked_TechPreviewEng ()
    {
        std::cout << "clicked - TECH-PREVIEW-eng!" << std::endl;
        buttonTechPreviewEng->setStyleSheet(Style::EnvButtons::ButtonActiv);
        buttonTechPreviewDe->setStyleSheet(Style::EnvButtons::ButtonInactive);
        buttonTechPreviewDeVoll->setStyleSheet(Style::EnvButtons::ButtonInactive);
        selection = "Eng";
    }
    void clicked_TechPreviewDe ()
    {
        std::cout << "clicked - TECH-PREVIEW-de!" << std::endl;
        buttonTechPreviewEng->setStyleSheet(Style::EnvButtons::ButtonInactive);
        buttonTechPreviewDe->setStyleSheet(Style::EnvButtons::ButtonActiv);
        buttonTechPreviewDeVoll->setStyleSheet(Style::EnvButtons::ButtonInactive);
        selection = "De";
    }
    void clicked_TechPreviewDeFull ()
    {
        std::cout << "clicked - TECH-PREVIEW-de-voll!" << std::endl;
        buttonTechPreviewEng->setStyleSheet(Style::EnvButtons::ButtonInactive);
        buttonTechPreviewDe->setStyleSheet(Style::EnvButtons::ButtonInactive);
        buttonTechPreviewDeVoll->setStyleSheet(Style::EnvButtons::ButtonActiv);
        selection = "DeVoll";
    }

    bool updateFiles ()
    {
        if(!isInstalled) return false;

        std::cout << "[TECH-PREVIEW] update selection: " << selection << std::endl;
        if(selection == "Eng")
        {
            return true;
        }
        else if(selection == "De")
        {
            return true;
        }
        else if(selection == "DeVoll")
        {
            return true;
        }
        else
        {
            return false;
        }
    }
};
