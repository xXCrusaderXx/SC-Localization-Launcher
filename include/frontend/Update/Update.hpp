#pragma once

#include <QGridLayout>
#include <QPushButton>
#include <QWidget>
#include <QtConcurrent/QtConcurrent>
#include <iostream>
#include <nlohmann/json.hpp>

#include "frontend/styles.hpp"

class UpdateButton : public QWidget
{
    Q_OBJECT

   private:
    std::string channel = "UPDATE";
    using Callback = std::function<void(const nlohmann::json &)>;
    Callback updateBackendCallback;

    QPushButton *buttonUpdate = nullptr;

    void clicked_start ()
    {
        nlohmann::json msg;
        msg["start"] = true;
        QtConcurrent::run([this, msg] { updateBackendCallback(msg); });
    }

   public:
    UpdateButton (QWidget *parent = nullptr, QGridLayout *grid = nullptr, int row = 0)
        : QWidget(parent)
    {
        buttonUpdate = new QPushButton("Nicht Bereit", this);
        connect(buttonUpdate, &QPushButton::clicked, this, &UpdateButton::clicked_start);
        grid->addWidget(buttonUpdate, 7, 0, 1, 2);
        buttonUpdate->setStyleSheet(ButtonStyle::Disabled);
        buttonUpdate->setDisabled(true);
    }

    virtual ~UpdateButton () = default;

    std::string getName () { return channel; }

    void updateStatus (const nlohmann::json &msg)
    {
        std::cout << msg.dump(4) << std::endl;
        if(msg.contains("buttonEnabled") && msg.at("buttonEnabled") == true)
        {
            buttonUpdate->setDisabled(false);
            buttonUpdate->setStyleSheet(ButtonStyle::Inactive);
        }
        if(msg.contains("buttonReady") && msg.at("buttonReady") == true)
        {
            buttonUpdate->setDisabled(false);
            buttonUpdate->setStyleSheet(ButtonStyle::Info);
            buttonUpdate->setText("Update Übersetzung");
        }
        if(msg.contains("buttonBusy") && msg.at("buttonBusy") == true)
        {
            buttonUpdate->setStyleSheet(ButtonStyle::Active);
            buttonUpdate->setText("Aktualisiere...");
            buttonUpdate->setDisabled(true);
        }
        if(msg.contains("buttonDisabled") && msg.at("buttonDisabled") == true)
        {
            buttonUpdate->setDisabled(true);
            buttonUpdate->setStyleSheet(ButtonStyle::Disabled);
            buttonUpdate->setText("Nicht Bereit");
        }
        if(msg.contains("LaunchScAfterTranslation"))
        {
            if(msg["LaunchScAfterTranslation"] == true)
                buttonUpdate->setText("Update und Start");
            else
                buttonUpdate->setText("Update Übersetzung");
        }
    }

    void setUpdateBackendCallback (Callback cb) { updateBackendCallback = cb; }
};
