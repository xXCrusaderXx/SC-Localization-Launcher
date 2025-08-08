#pragma once

#include <QGridLayout>
#include <QPushButton>
#include <QWidget>
#include <QtConcurrent/QtConcurrent>
#include <iostream>
#include <nlohmann/json.hpp>

#include "frontend/styles.hpp"
#include "logging/logging.h"
#include "protocol/Protocol.hpp"
#include "protocol/UpdateButtonPayload.hpp"

class UpdateButton : public QWidget
{
    Q_OBJECT

   private:
    std::string channel = "UPDATE";
    using Callback = std::function<void(const nlohmann::json &)>;
    Callback updateBackendCallback;
    LoggerFramework::LogEx lg;

    QPushButton *buttonUpdate = nullptr;

    void clicked_start ()
    {
        Protocol::UpdateButtonPayload updateButtonRequest;
        updateButtonRequest.updateButton.updateButtonClicked = true;

        Protocol::Massage request(Protocol::MassageType::Request);
        request.AddModuleNode("UPDATE", updateButtonRequest.toJson());
        QtConcurrent::run([this, request] { updateBackendCallback(request.getJson()); });
    }

   public:
    UpdateButton (QWidget *parent = nullptr, QGridLayout *grid = nullptr, int row = 0)
        : QWidget(parent)
        , lg("UpdateButton")
    {
        buttonUpdate = new QPushButton("Nicht Bereit", this);
        connect(buttonUpdate, &QPushButton::clicked, this, &UpdateButton::clicked_start);
        grid->addWidget(buttonUpdate, 7, 0, 1, 2);
        buttonUpdate->setStyleSheet(ButtonStyle::Disabled);
        buttonUpdate->setDisabled(true);
    }

    virtual ~UpdateButton () = default;

    std::string getName () { return channel; }

    void updateStatus (const nlohmann::json &response)
    {
        if(QThread::currentThread() != this->thread())
        {
            QMetaObject::invokeMethod(this, [this, response] () { updateStatus(response); }, Qt::QueuedConnection);
            return;
        }
        LOG_DEBUG(lg) << "updateStatus() - START";
        Protocol::UpdateButtonPayload updateButtonResponse(response);
        LOG_DEBUG(lg) << "updateStatus() - msg: " << updateButtonResponse.toJson().dump(4);

        if(updateButtonResponse.updateButton.enabled)
        {
            if(updateButtonResponse.updateButton.enabled.value())
            {
                buttonUpdate->setVisible(true);
            }
            else
            {
                buttonUpdate->setVisible(false);
            }
        }
        if(updateButtonResponse.updateButton.busy)
        {
            if(updateButtonResponse.updateButton.busy.value())
            {
                buttonUpdate->setDisabled(true);
                buttonUpdate->setStyleSheet(ButtonStyle::Active);
                buttonUpdate->setText("Aktualisiere...");
            }
            else
            {
                buttonUpdate->setDisabled(false);
                buttonUpdate->setStyleSheet(ButtonStyle::Info);
                buttonUpdate->setText("Update Übersetzung");
            }
        }

        if(updateButtonResponse.updateButton.active)
        {
            if(updateButtonResponse.updateButton.active.value())
            {
                buttonUpdate->setDisabled(false);
                buttonUpdate->setStyleSheet(ButtonStyle::Info);
                buttonUpdate->setText("Update Übersetzung");
            }
            else
            {
                buttonUpdate->setDisabled(true);
                buttonUpdate->setStyleSheet(ButtonStyle::Disabled);
                buttonUpdate->setText("Nicht Bereit");
            }
        }
        if(updateButtonResponse.updateButton.LaunchScAfterTranslation)
        {
            if(updateButtonResponse.updateButton.LaunchScAfterTranslation.value())
            {
                buttonUpdate->setText("Update und Start");
            }
            else
            {
                buttonUpdate->setText("Update Übersetzung");
            }
        }

        LOG_DEBUG(lg) << "updateStatus() - FINISHED";
    }

    void setUpdateBackendCallback (Callback cb) { updateBackendCallback = cb; }
};
