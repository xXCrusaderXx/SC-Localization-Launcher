#pragma once

#include <QFileDialog>
#include <QGridLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QThreadPool>
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

            QThreadPool::globalInstance()->start([request, this] { updateBackendCallback(request.getJson()); });
        }
    }

    // Only updates the button look; does not notify the backend
    void showSelection (QPushButton *selected)
    {
        for(QPushButton *button : {buttonEng, buttonDe, buttonDeVoll})
        {
            button->setStyleSheet(button == selected ? ButtonStyle::Active : ButtonStyle::Inactive);
            button->setDisabled(button == selected);
        }
    }

    void sendSelection (bool eng, bool de, bool deFull)
    {
        Protocol::ChannelPayload channelRequest;
        channelRequest.buttonEng.selected = eng;
        channelRequest.buttonDe.selected = de;
        channelRequest.buttonDeFull.selected = deFull;

        Protocol::Massage request(Protocol::MassageType::Request);
        request.AddModuleNode(channel, channelRequest.toJson());
        QThreadPool::globalInstance()->start([request, this] { updateBackendCallback(request.getJson()); });
    }

    void clicked_Eng ()
    {
        LOG_DEBUG(lg) << "clicked_Eng()";
        showSelection(buttonEng);
        sendSelection(true, false, false);
    }

    void clicked_De ()
    {
        LOG_DEBUG(lg) << "clicked_De()";
        showSelection(buttonDe);
        sendSelection(false, true, false);
    }

    void clicked_DeFull ()
    {
        LOG_DEBUG(lg) << "clicked_DeFull()";
        showSelection(buttonDeVoll);
        sendSelection(false, false, true);
    }

   public:
    explicit ChannelBase (const std::string &channelName, QWidget *parent = nullptr, QGridLayout *grid = nullptr, int row = 0)
        : QWidget(parent)
        , lg(channelName + "-Channel")
        , channel(channelName)
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
                showSelection(buttonEng);
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
                showSelection(buttonDe);
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
                showSelection(buttonDeVoll);
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
