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
#include "protocol/ChannelPayload.hpp"
#include "protocol/Protocol.hpp"

class ChannelBase : public QWidget
{
    // Q_OBJECT
   private:
    LoggerFramework::LogEx lg;

   protected:
    std::string channel;

    using Callback = std::function<void(const nlohmann::json &)>;
    Callback updateBackendCallback;

    QPushButton *buttonChannel = nullptr;
    QPushButton *buttonEng = nullptr;
    QPushButton *buttonDe = nullptr;
    QPushButton *buttonDeVoll = nullptr;

    bool buttonEngSelected = false;
    bool buttonDeSelected = false;
    bool buttonDeVollSelected = false;

    void clicked_Channel ()
    {
        QMessageBox::information(nullptr, "Star Citizen Installation",
                                 QString("Gib den Dateipfad zu deiner %1 Installation an!").arg(QString::fromStdString(channel)));

        std::string selectedDir =
            QString(QFileDialog::getExistingDirectory(nullptr, "Ordner auswählen", QDir::homePath(), QFileDialog::ShowDirsOnly)).toStdString();

        if(!selectedDir.empty())
        {
            Protocol::Request request;
            Protocol::ChannelPayload::Request channelRequest;

            channelRequest.newInstallPath = std::filesystem::path(selectedDir);
            channelRequest.buttonEng.enabled = buttonEng->isEnabled();
            channelRequest.buttonDe.enabled = buttonDe->isEnabled();
            channelRequest.buttonDeFull.enabled = buttonDeVoll->isEnabled();

            channelRequest.buttonEng.selected = buttonEngSelected;
            channelRequest.buttonDe.selected = buttonDeSelected;
            channelRequest.buttonDeFull.selected = buttonDeVollSelected;

            request.payload = channelRequest.to_json();
            LOG_DEBUG(lg) << "clicked_Channel() - request:\n" << request.toJson().dump(4);

            request.type = Protocol::payloadTypeFromString(channel);

            QtConcurrent::run([request, this] { updateBackendCallback(request.toJson()); });
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
        QtConcurrent::run([msg, this] { updateBackendCallback(msg); });
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
        QtConcurrent::run([msg, this] { updateBackendCallback(msg); });
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
        QtConcurrent::run([msg, this] { updateBackendCallback(msg); });
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
        Protocol::Response response;
        response.fromJson(msg);
        Protocol::ChannelPayload::Response channelResponse;
        channelResponse.from_json(response.payload);

        if(QThread::currentThread() != this->thread())
        {
            QMetaObject::invokeMethod(this, [this, msg] () { updateStatus(msg); }, Qt::QueuedConnection);
            return;
        }

        LOG_DEBUG(lg) << "updateStatus() - msg:\n" << msg.dump(4);

        buttonDeSelected = channelResponse.buttonDe.selected;
        buttonEngSelected = channelResponse.buttonEng.selected;
        buttonDeVollSelected = channelResponse.buttonDeFull.selected;

        if(channelResponse.buttonChannel.enabled)
        {
            buttonChannel->setVisible(true);
            buttonChannel->setDisabled(false);
            if(channelResponse.buttonChannel.acitve)
            {
                buttonChannel->setStyleSheet(ButtonStyle::Info);
            }
            else
            {
                buttonChannel->setStyleSheet(ButtonStyle::Error);
            }
        }
        else
        {
            buttonChannel->setVisible(false);
            buttonChannel->setDisabled(true);
        }

        if(channelResponse.buttonEng.enabled)
        {
            buttonEng->setVisible(true);
            if(channelResponse.buttonEng.selected)
            {
                clicked_Eng();
            }
        }
        else
        {
            buttonEng->setVisible(false);
        }

        if(channelResponse.buttonDe.enabled)
        {
            buttonEng->setVisible(true);
            if(channelResponse.buttonDe.selected)
            {
                clicked_De();
            }
        }
        else
        {
            buttonDe->setVisible(false);
        }

        if(channelResponse.buttonDeFull.enabled)
        {
            buttonDeVoll->setVisible(true);
            if(channelResponse.buttonDeFull.selected)
            {
                clicked_DeFull();
            }
        }
        else
        {
            buttonDeVoll->setVisible(false);
        }

        if(channelResponse.installPathIsSet)
        {
            buttonChannel->setStyleSheet(ButtonStyle::Info);
        }
    }

    void setUpdateBackendCallback (Callback cb) { updateBackendCallback = cb; }
};
