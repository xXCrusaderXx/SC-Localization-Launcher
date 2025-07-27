#pragma once

#include <QPushButton>
#include <QWidget>
#include <nlohmann/json.hpp>

#include "frontend/styles.hpp"

class UpdateButton : public QWidget
{
    // Q_OBJECT

   private:
    std::string channel = "UPDATE";
    using Callback = std::function<void(const std::string key, const nlohmann::json &)>;
    Callback updateBackendCallback;

    QPushButton *buttonUpdate = nullptr;

    void clicked ()
    {
        nlohmann::json msg;
        msg["start"] = true;
        QtConcurrent::run([=] { updateBackendCallback(channel, msg); });
    }

   public:
    UpdateButton (QWidget *parent = nullptr, QGridLayout *grid = nullptr, int row = 0)
        : QWidget(parent)
    {
        buttonUpdate = new QPushButton("Nicht Bereit", this);
        connect(buttonUpdate, &QPushButton::clicked, this, &UpdateButton::clicked);
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
    }

    void setUpdateBackendCallback (Callback cb) { updateBackendCallback = cb; }
};
