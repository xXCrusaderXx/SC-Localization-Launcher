#pragma once

#include <QWidget>

#include "logging/logging.h"

class MenuBar : public QWidget
{
   private:
    LoggerFramework::LogEx lg;

    const QString menuButtonStyle = R"(
    QPushButton {
        background-color: transparent;
        border: none;
    }
    QPushButton:hover {
        background-color: rgba(255, 255, 255, 0.1);
    }
    QPushButton:pressed {
        background-color: rgba(255, 255, 255, 0.2);
    }
)";

    QPushButton *buttonNotifications = nullptr;
    QPushButton *buttonMinimize = nullptr;
    QPushButton *buttonMaximize = nullptr;
    QPushButton *buttonClose = nullptr;

    void clicked_Notification () { LOG_DEBUG(lg) << "clicked_Notification()"; }

    void clicked_Minimize ()
    {
        LOG_DEBUG(lg) << "clicked_Minimize()";
        if(parentWidget())
        {
            parentWidget()->showMinimized();
        }
    }

    void clicked_Maximize () { LOG_DEBUG(lg) << "clicked_Maximize()"; }

    void clicked_Close ()
    {
        LOG_DEBUG(lg) << "clicked_Close()";
        if(parentWidget())
        {
            parentWidget()->close();
        }
    }

   public:
    MenuBar (QWidget *parent = nullptr, QGridLayout *grid = nullptr)
        : QWidget(parent)
        , lg("MenuBar")
    {
        buttonNotifications = new QPushButton(u8"\U0001F514", this);
        QFont fontNotify = buttonNotifications->font();
        fontNotify.setPointSize(20);
        buttonNotifications->setFont(fontNotify);

        buttonMinimize = new QPushButton(u8"\u2015", this);
        buttonMaximize = new QPushButton(u8"\u25A1", this);
        buttonClose = new QPushButton(u8"\u2715", this);

        buttonNotifications->setFixedSize(30, 30);
        buttonMinimize->setFixedSize(30, 30);
        buttonMaximize->setFixedSize(30, 30);
        buttonClose->setFixedSize(30, 30);

        buttonNotifications->setStyleSheet(menuButtonStyle);
        buttonMinimize->setStyleSheet(menuButtonStyle);
        buttonMaximize->setStyleSheet(menuButtonStyle);
        buttonClose->setStyleSheet(menuButtonStyle);

        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 0);
        grid->setColumnStretch(2, 0);
        grid->setColumnStretch(3, 0);

        grid->addWidget(buttonNotifications, 1, 1);
        grid->addWidget(buttonMinimize, 1, 2);
        grid->addWidget(buttonMaximize, 1, 3);
        grid->addWidget(buttonClose, 1, 4);

        connect(buttonNotifications, &QPushButton::clicked, this, &MenuBar::clicked_Notification);
        connect(buttonMinimize, &QPushButton::clicked, this, &MenuBar::clicked_Minimize);
        connect(buttonMaximize, &QPushButton::clicked, this, &MenuBar::clicked_Maximize);
        connect(buttonClose, &QPushButton::clicked, this, &MenuBar::clicked_Close);

        // buttonMinimize->setStyleSheet(Style::EnvButtons::Button);
        // buttonMaximize->setStyleSheet(ButtonStyle::Disabled);
        // buttonClose->setStyleSheet(ButtonStyle::Disabled);

        buttonMaximize->setDisabled(true);

        LOG_DEBUG(lg) << "instanziated";
    }

    virtual ~MenuBar () { LOG_DEBUG(lg) << "destructed"; }
};
