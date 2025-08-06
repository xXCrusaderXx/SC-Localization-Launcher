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

    void clicked_Channel ()
    {
        QMessageBox::information(nullptr, "Star Citizen Installation",
                                 QString("Gib den Dateipfad zu deiner %1 Installation an!").arg(QString::fromStdString(channel)));

        std::string selectedDir =
            QString(QFileDialog::getExistingDirectory(nullptr, "Ordner auswählen", QDir::homePath(), QFileDialog::ShowDirsOnly)).toStdString();

        if(!selectedDir.empty())
        {
            Protocol::ChannelPayload channelRequest;

            channelRequest.newInstallPath = std::filesystem::path(selectedDir);

            Protocol::Massage request(Protocol::MassageType::Request);
            request.AddModuleNode(channel, channelRequest.toJson());
            LOG_DEBUG(lg) << "clicked_Channel() - request:\n" << request.getJson().dump(4);

            QtConcurrent::run([request, this] { updateBackendCallback(request.getJson()); });
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

        Protocol::ChannelPayload channelRequest;
        channelRequest.buttonEng.selected = true;
        channelRequest.buttonDe.selected = false;
        channelRequest.buttonDeFull.selected = false;

        Protocol::Massage request(Protocol::MassageType::Request);
        request.AddModuleNode(channel, channelRequest.toJson());
        QtConcurrent::run([request, this] { updateBackendCallback(request.getJson()); });
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

        Protocol::ChannelPayload channelRequest;
        channelRequest.buttonEng.selected = false;
        channelRequest.buttonDe.selected = true;
        channelRequest.buttonDeFull.selected = false;

        Protocol::Massage request(Protocol::MassageType::Request);
        request.AddModuleNode(channel, channelRequest.toJson());
        QtConcurrent::run([request, this] { updateBackendCallback(request.getJson()); });
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

        Protocol::ChannelPayload channelRequest;
        channelRequest.buttonEng.selected = false;
        channelRequest.buttonDe.selected = false;
        channelRequest.buttonDeFull.selected = true;

        Protocol::Massage request(Protocol::MassageType::Request);
        request.AddModuleNode(channel, channelRequest.toJson());
        QtConcurrent::run([request, this] { updateBackendCallback(request.getJson()); });
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
        LOG_DEBUG(lg) << "updateStatus() - [" << channel << "] - START";
        LOG_DEBUG(lg) << "updateStatus() - msg:\n" << msg.dump(4);
        Protocol::ChannelPayload channelResponse(msg);

        if(QThread::currentThread() != this->thread())
        {
            QMetaObject::invokeMethod(this, [this, msg] () { updateStatus(msg); }, Qt::QueuedConnection);
            return;
        }

        if(channelResponse.buttonChannel.enabled)
        {
            if(channelResponse.buttonChannel.enabled.value())
            {
                buttonChannel->setVisible(true);
                buttonChannel->setDisabled(false);
            }
            else
            {
                buttonChannel->setVisible(false);
                buttonChannel->setDisabled(true);
            }
        }

        if(channelResponse.buttonChannel.active)
        {
            if(channelResponse.buttonChannel.active.value())
            {
                buttonChannel->setStyleSheet(ButtonStyle::Info);
            }
            else
            {
                buttonChannel->setStyleSheet(ButtonStyle::Error);
            }
        }

        if(channelResponse.buttonEng.enabled)
        {
            if(channelResponse.buttonEng.enabled.value())
            {
                buttonEng->setVisible(true);
            }
            else
            {
                buttonEng->setVisible(false);
            }
        }

        if(channelResponse.buttonEng.selected)
        {
            if(channelResponse.buttonEng.selected.value())
            {
                clicked_Eng();
            }
        }

        if(channelResponse.buttonDe.enabled)
        {
            if(channelResponse.buttonDe.enabled.value())
            {
                buttonDe->setVisible(true);
            }
            else
            {
                buttonDe->setVisible(false);
            }
        }

        if(channelResponse.buttonDe.selected)
        {
            if(channelResponse.buttonDe.selected.value())
            {
                clicked_De();
            }
        }
        if(channelResponse.buttonDeFull.enabled)
        {
            if(channelResponse.buttonDeFull.enabled.value())
            {
                buttonDeVoll->setVisible(true);
            }
            else
            {
                buttonDeVoll->setVisible(false);
            }
        }

        if(channelResponse.buttonDeFull.selected)
        {
            if(channelResponse.buttonDeFull.selected.value())
            {
                clicked_DeFull();
            }
        }

        if(channelResponse.installPathIsSet)
        {
            if(channelResponse.installPathIsSet.value())
            {
                buttonChannel->setStyleSheet(ButtonStyle::Info);
            }
        }
        LOG_DEBUG(lg) << "updateStatus() - [" << channel << "] - FINISHED";
    }

    void setUpdateBackendCallback (Callback cb) { updateBackendCallback = cb; }
};
