#pragma once

#include <QGridLayout>
#include <QPushButton>
#include <QWidget>
#include <nlohmann/json.hpp>

#include "SettingsData.hpp"
#include "SettingsWindow.hpp"
#include "logging/logging.h"
#include "protocol/Protocol.hpp"
#include "protocol/SettingsPayload.hpp"

class Settings : public QWidget
{
    Q_OBJECT

   private:
    LoggerFramework::LogEx lg;
    std::string channel;
    std::unique_ptr<SettingsWindow> settingsWindow;
    SettingsData settingsData;

    using Callback = std::function<void(const std::string key, const nlohmann::json &)>;
    Callback updateBackendCallback;

    const QString settingsButtonStyle = R"(
    QPushButton {
        background-color: transparent;
        text-align: right;
        border-radius: 4px;
        padding: 0px 4px 0px 4px; /* oben, rechts, unten, links */
        qproperty-iconSize: 30px;
        border: none;
    }
)";

    QPushButton *buttonSettings = nullptr;

    void clicked_Settings ()
    {
        LOG_DEBUG(lg) << "clicked_Settings()";
        if(!settingsWindow || !settingsWindow->isVisible())
        {
            settingsWindow = std::make_unique<SettingsWindow>(this, settingsData);
            settingsWindow->setSettingsCallback(
                [this] (const nlohmann::json &msg)
                {
                    Protocol::Massage request(Protocol::MassageType::Request);
                    request.AddModuleNode(channel, msg);
                    updateBackendCallback(channel, request.getJson());
                });
            settingsWindow->show();
        }
        else
        {
            settingsWindow->raise();
            settingsWindow->activateWindow();
        }
    }

   public:
    explicit Settings (QWidget *parent = nullptr, QGridLayout *grid = nullptr)
        : QWidget(parent)
        , lg("SETTINGS")
        , channel("SETTINGS")
    {
        buttonSettings = new QPushButton("Einstellungen", this);

        buttonSettings->setLayoutDirection(Qt::RightToLeft);

        QIcon icon(":/settings.png");
        if(icon.isNull())
        {
            LOG_DEBUG(lg) << ":/settings.png konnte nicht geladen werden!";
        }
        else
        {
            buttonSettings->setIcon(QIcon(":/settings.png"));
        }

        buttonSettings->setIconSize(QSize(30, 30));
        QFont font = buttonSettings->font();
        font.setPointSize(12);
        buttonSettings->setFont(font);
        buttonSettings->setStyleSheet(settingsButtonStyle);

        connect(buttonSettings, &QPushButton::clicked, this, &Settings::clicked_Settings);

        grid->setColumnStretch(3, 1);
        grid->addWidget(buttonSettings, 0, 4);

        LOG_DEBUG(lg) << "instanziated";
    }

    virtual ~Settings () { LOG_DEBUG(lg) << "destructed"; }

    void setUpdateBackendCallback (Callback cb) { updateBackendCallback = cb; }

    std::string getName () { return channel; }

    void updateStatus (const nlohmann::json &msg)
    {
        if(QThread::currentThread() != this->thread())
        {
            QMetaObject::invokeMethod(this, [this, msg] () { updateStatus(msg); }, Qt::QueuedConnection);
            return;
        }
        Protocol::SettingsPayload settingsResponse(msg);
        LOG_DEBUG(lg) << "updateStatus() - START";
        LOG_DEBUG(lg) << "updateStatus() - msg:\n" << settingsResponse.toJson().dump(4);

        settingsData.updateSettingsData(settingsResponse);

        if(settingsWindow)
        {
            settingsWindow->updateSettingsWindow(settingsData);
        }
        LOG_DEBUG(lg) << "Settings-Status: update-finished";
    }
};
