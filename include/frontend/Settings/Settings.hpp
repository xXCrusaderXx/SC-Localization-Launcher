#pragma once

// #include <QDialog>
//  #include <QEvent>
#include <QGridLayout>
#include <QPushButton>
#include <QWidget>
// #include <QVBoxLayout>
// #include <QWindow>

#include <nlohmann/json.hpp>
#include "SettingsWindow.hpp"
#include "logging/logging.h"

class Settings : public QWidget
{
    Q_OBJECT

   private:
    LoggerFramework::LogEx lg;
    std::string channel;
    std::unique_ptr<SettingsWindow> settingsWindow;
    nlohmann::json settingsStatus;

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
            settingsWindow = std::make_unique<SettingsWindow>(this, settingsStatus);
            settingsWindow->setSettingsCallback(
                [this] (const nlohmann::json &msg)
                {
                    updateBackendCallback(channel, msg);
                    settingsStatus = msg;
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
        buttonSettings->setIcon(QIcon(":/settings.png"));
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
        LOG_DEBUG(lg) << "updateStatus() - msg:\n" << msg.dump(4);
        settingsStatus = msg;
    }
};
