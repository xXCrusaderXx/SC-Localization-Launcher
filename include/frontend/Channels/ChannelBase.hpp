#pragma once

#include <QFileDialog>
#include <QGridLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QWidget>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

#include "backend/utils/paths.hpp"
#include "backend/utils/utils.hpp"
#include "frontend/styles.hpp"
#include "logging/logging.h"

class ChannelBase : public QWidget
{
    // Q_OBJECT
   private:
    LoggerFramework::LogEx lg;

   protected:
    std::string channel;

    using Callback = std::function<void(const std::string key, const nlohmann::json &)>;
    Callback updateBackendCallback;

    QPushButton *buttonChannel = nullptr;
    QPushButton *buttonEng = nullptr;
    QPushButton *buttonDe = nullptr;
    QPushButton *buttonDeVoll = nullptr;

    void clicked_Channel ()
    {
        QMessageBox::information(nullptr, "Star Citizen Installation",
                                 QString("Gib den Dateipfad zu deiner %1 Installation an!").arg(QString::fromStdString(channel)));

        std::string selectedDir =
            QString(QFileDialog::getExistingDirectory(nullptr, "Ordner auswählen", QDir::homePath(), QFileDialog::ShowDirsOnly)).toStdString();

        if(!selectedDir.empty())
        {
            nlohmann::json msg;
            msg["installPath"] = selectedDir;
            QtConcurrent::run([=] { updateBackendCallback(channel, msg); });
        }
    }

    void clicked_Eng ()
    {
        LOG_DEBUG(lg) << "clicked_Eng()";
        buttonEng->setStyleSheet(ButtonStyle::Active);
        buttonDe->setStyleSheet(ButtonStyle::Inactive);
        buttonDeVoll->setStyleSheet(ButtonStyle::Inactive);

        buttonEng->setVisible(true);

        buttonEng->setDisabled(true);
        buttonDe->setDisabled(false);
        buttonDeVoll->setDisabled(false);

        nlohmann::json msg;
        msg["buttonEngSelected"] = true;
        msg["buttonDeSelected"] = false;
        msg["buttonDeFullSelected"] = false;
        QtConcurrent::run([=] { updateBackendCallback(channel, msg); });
    }

    void clicked_De ()
    {
        LOG_DEBUG(lg) << "clicked_De()";
        buttonEng->setStyleSheet(ButtonStyle::Inactive);
        buttonDe->setStyleSheet(ButtonStyle::Active);
        buttonDeVoll->setStyleSheet(ButtonStyle::Inactive);

        buttonEng->setDisabled(false);
        buttonDe->setDisabled(true);
        buttonDeVoll->setDisabled(false);

        nlohmann::json msg;
        msg["buttonEngSelected"] = false;
        msg["buttonDeSelected"] = true;
        msg["buttonDeFullSelected"] = false;
        QtConcurrent::run([=] { updateBackendCallback(channel, msg); });
    }

    void clicked_DeFull ()
    {
        LOG_DEBUG(lg) << "clicked_DeFull()";
        buttonEng->setStyleSheet(ButtonStyle::Inactive);
        buttonDe->setStyleSheet(ButtonStyle::Inactive);
        buttonDeVoll->setStyleSheet(ButtonStyle::Active);

        buttonEng->setDisabled(false);
        buttonDe->setDisabled(false);
        buttonDeVoll->setDisabled(true);

        nlohmann::json msg;
        msg["buttonEngSelected"] = false;
        msg["buttonDeSelected"] = false;
        msg["buttonDeFullSelected"] = true;
        QtConcurrent::run([=] { updateBackendCallback(channel, msg); });
    }

   public:
    explicit ChannelBase (const std::string &channelName, QWidget *parent = nullptr, QGridLayout *grid = nullptr, int row = 0)
        : QWidget(parent)
        , channel(channelName)
        , lg(channelName + "-Channel")
    {
        buttonChannel = new QPushButton(QString::fromStdString(channel), this);
        buttonEng = new QPushButton("eng", this);
        buttonDe = new QPushButton("de", this);
        buttonDeVoll = new QPushButton("deVoll", this);

        connect(buttonChannel, &QPushButton::clicked, this, &ChannelBase::clicked_Channel);
        connect(buttonEng, &QPushButton::clicked, this, &ChannelBase::clicked_Eng);
        connect(buttonDe, &QPushButton::clicked, this, &ChannelBase::clicked_De);
        connect(buttonDeVoll, &QPushButton::clicked, this, &ChannelBase::clicked_DeFull);

        buttonChannel->setStyleSheet(ButtonStyle::Error);
        buttonEng->setStyleSheet(ButtonStyle::Disabled);
        buttonDe->setStyleSheet(ButtonStyle::Disabled);
        buttonDeVoll->setStyleSheet(ButtonStyle::Disabled);

        buttonEng->setDisabled(true);
        buttonDe->setDisabled(true);
        buttonDeVoll->setDisabled(true);

        buttonEng->setVisible(false);
        buttonDe->setVisible(false);
        buttonDeVoll->setVisible(false);

        LOG_DEBUG(lg) << "instanziated";
    }

    virtual ~ChannelBase () { LOG_DEBUG(lg) << "destructed"; }

    std::string getName () { return channel; }

    void updateStatus (const nlohmann::json &msg)
    {
        LOG_DEBUG(lg) << "updateStatus() - msg:\n" << msg.dump(4);
        if(msg.contains("buttonEngEnabled") && msg.at("buttonEngEnabled") == true)
        {
            buttonEng->setVisible(true);
        }
        if(msg.contains("buttonDeEnabled") && msg.at("buttonDeEnabled") == true)
        {
            buttonDe->setVisible(true);
        }
        if(msg.contains("buttonDeFullEnabled") && msg.at("buttonDeFullEnabled") == true)
        {
            buttonDeVoll->setVisible(true);
        }

        if(msg.contains("buttonEngSelected") && msg.at("buttonEngSelected") == true) clicked_Eng();
        if(msg.contains("buttonDeSelected") && msg.at("buttonDeSelected") == true) clicked_De();
        if(msg.contains("buttonDeFullSelected") && msg.at("buttonDeFullSelected") == true) clicked_DeFull();

        if(msg.contains("installPathIsSet") && msg.at("installPathIsSet") == true)
        {
            buttonChannel->setStyleSheet(ButtonStyle::Info);
        }
    }

    void setUpdateBackendCallback (Callback cb) { updateBackendCallback = cb; }
};
