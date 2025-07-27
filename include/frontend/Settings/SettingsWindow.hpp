#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

#include <nlohmann/json.hpp>
#include "logging/logging.h"

class SettingsWindow : public QWidget
{
    Q_OBJECT

   private:
    LoggerFramework::LogEx lg;

    const QString labelStyle = R"(
    QLabel {
        background-color: rgba(255, 255, 255, 0.2);
    })";

    const QString checkBoxStyle = R"(
    QCheckBox::indicator {
    width: 15px;
    height: 15;
    border: 2px solid rgba(66, 66, 66, 1);
    border-radius: 5px;
    background-color: rgba(255, 255, 255, 1);
}

QCheckBox::indicator:checked {
    background-color: rgba(0, 120, 215, 1);
    border-color: rgba(0, 89, 153, 1);
}
)";

    const QString folderButtonStyle = R"(
    QPushButton {
        font-size: 30px;
        background-color: transparent;
        border: none;
        color: white;
        padding: 0px 0px;
    }
    QPushButton:hover {
        background-color: rgba(200, 200, 200, 0.15); /* leicht grauer Hover-Hintergrund */
        color: white; /* bleibt weiß */
    }
    QPushButton:pressed {
        background-color: rgba(150, 150, 150, 0.3); /* dunkleres Grau beim Drücken */
        color: white;
    }
)";

    void clicked_Close ()
    {
        LOG_DEBUG(lg) << "clicked_Close()";
        if(this)
        {
            this->close();
        }
    }

   protected:
    bool eventFilter (QObject *obj, QEvent *event) override;

   public:
    explicit SettingsWindow (QWidget *parent)
        : QWidget(parent)
        , lg("SettingsWindow")
    {
        this->setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
        resize(600, 500);
        installEventFilter(this);

        this->setStyleSheet(R"(
    QWidget {
        background-color: rgba(28, 30, 33, 1);
    }
)");
        auto *mainLayout = new QGridLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);

        setupTopWidget(mainLayout);
        setupTopLeftWidget(mainLayout);
        setupTopRightWidget(mainLayout);
        setupBottomWidget(mainLayout);

        LOG_DEBUG(lg) << "instanziated";
    }

    void setupTopWidget (QGridLayout *grid)
    {
        auto *topMenuBar = new QWidget(this);
        auto *topMenuLayout = new QGridLayout(topMenuBar);
        topMenuLayout->setContentsMargins(5, 5, 5, 5);
        grid->addWidget(topMenuBar, 0, 0, 1, 2);

        auto *labeltitel = new QLabel("Einstellungen", topMenuBar);
        topMenuLayout->addWidget(labeltitel, 1, 0);

        auto *buttonClose = new QPushButton(u8"\u2715", topMenuBar);
        buttonClose->setFixedSize(30, 30);
        buttonClose->setStyleSheet(R"(
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
)");
        topMenuLayout->setColumnStretch(0, 1);
        topMenuLayout->setColumnStretch(1, 0);
        topMenuLayout->setColumnStretch(2, 0);
        topMenuLayout->setColumnStretch(3, 0);

        topMenuLayout->addWidget(buttonClose, 1, 2);

        connect(buttonClose, &QPushButton::clicked, this, &SettingsWindow::clicked_Close);
    }

    void setupTopLeftWidget (QGridLayout *grid)
    {
        QWidget *topLeftWidget = new QWidget(this);
        topLeftWidget->setMinimumSize(1, 2);
        grid->addWidget(topLeftWidget, 1, 0, 1, 1);
        QGridLayout *layoutTopLeft = new QGridLayout(topLeftWidget);
        topLeftWidget->setLayout(layoutTopLeft);

        // u8"2611" 2713,2714
        // u8"2610"

        QCheckBox *checkbox1 = new QCheckBox("Auto. Übersetzungs-Update beim SCD Launcher Start", topLeftWidget);
        checkbox1->setStyleSheet(checkBoxStyle);
        QCheckBox *checkbox2 = new QCheckBox("Neue Übersetzung immer automatisch aktualisieren", topLeftWidget);
        checkbox2->setStyleSheet(checkBoxStyle);
        QCheckBox *checkbox3 = new QCheckBox("SCD Launcher nach Updates in Tray minimieren", topLeftWidget);
        checkbox3->setStyleSheet(checkBoxStyle);
        QCheckBox *checkbox4 = new QCheckBox("RSI Launcher nach übersetzungs-update starten", topLeftWidget);
        checkbox4->setStyleSheet(checkBoxStyle);
        QCheckBox *checkbox5 = new QCheckBox("Autostart SCD Launcher mit Systemstart", topLeftWidget);
        checkbox5->setStyleSheet(checkBoxStyle);
        QCheckBox *checkbox6 = new QCheckBox("Updateprozess im SCD Launcher anzeigen", topLeftWidget);
        checkbox6->setStyleSheet(checkBoxStyle);

        layoutTopLeft->addWidget(checkbox1, 0, 0);
        layoutTopLeft->addWidget(checkbox2, 1, 0);
        layoutTopLeft->addWidget(checkbox3, 2, 0);
        layoutTopLeft->addWidget(checkbox4, 3, 0);
        layoutTopLeft->addWidget(checkbox5, 4, 0);
        layoutTopLeft->addWidget(checkbox6, 5, 0);

        layoutTopLeft->setRowStretch(6, 1);
    }

    void setupTopRightWidget (QGridLayout *grid)
    {
        QWidget *topRightWidget = new QWidget(this);
        topRightWidget->setMinimumSize(1, 2);
        grid->addWidget(topRightWidget, 1, 1, 1, 1);
        QGridLayout *layoutTopRight = new QGridLayout(topRightWidget);
        topRightWidget->setLayout(layoutTopRight);

        int labelX = 110;
        int labelY = 60;

        QLabel *label1 = new QLabel(topRightWidget);
        label1->setFixedSize(labelX, labelY);
        label1->setStyleSheet(labelStyle);
        QLabel *label2 = new QLabel(topRightWidget);
        label2->setFixedSize(labelX, labelY);
        label2->setStyleSheet(labelStyle);
        QLabel *label3 = new QLabel(topRightWidget);
        label3->setFixedSize(labelX, labelY);
        label3->setStyleSheet(labelStyle);
        QLabel *label4 = new QLabel(topRightWidget);
        label4->setFixedSize(labelX, labelY);
        label4->setStyleSheet(labelStyle);
        QLabel *label5 = new QLabel(topRightWidget);
        label5->setFixedSize(labelX, labelY);
        label5->setStyleSheet(labelStyle);
        QLabel *label6 = new QLabel(topRightWidget);
        label6->setFixedSize(labelX, labelY);
        label6->setStyleSheet(labelStyle);

        layoutTopRight->addWidget(label1, 0, 0);
        layoutTopRight->addWidget(label2, 0, 1);
        layoutTopRight->addWidget(label3, 1, 0);
        layoutTopRight->addWidget(label4, 1, 1);
        layoutTopRight->addWidget(label5, 2, 0);
        layoutTopRight->addWidget(label6, 2, 1);
    }

    void setupBottomWidget (QGridLayout *grid)
    {
        QWidget *topBottomWidget = new QWidget(this);
        topBottomWidget->setMinimumSize(1, 2);
        grid->addWidget(topBottomWidget, 2, 0, 1, 2);
        QGridLayout *layoutBottom = new QGridLayout(topBottomWidget);
        topBottomWidget->setLayout(layoutBottom);

        QLabel *label1 = new QLabel("Star Citizen Installationen", topBottomWidget);
        QLabel *label2 = new QLabel("LIVE", topBottomWidget);
        QLabel *label3 = new QLabel("PTU", topBottomWidget);
        QLabel *label4 = new QLabel("EPTU", topBottomWidget);
        QLabel *label5 = new QLabel("HOTFIX", topBottomWidget);
        QLabel *label6 = new QLabel("TECH-PREVIEW", topBottomWidget);
        QLabel *label7 = new QLabel("RSI-Launcher", topBottomWidget);

        QLineEdit *pathBox1 = new QLineEdit("Option 1", topBottomWidget);
        QLineEdit *pathBox2 = new QLineEdit("Option 2", topBottomWidget);
        QLineEdit *pathBox3 = new QLineEdit("Option 3", topBottomWidget);
        QLineEdit *pathBox4 = new QLineEdit("Option 4", topBottomWidget);
        QLineEdit *pathBox5 = new QLineEdit("Option 5", topBottomWidget);
        QLineEdit *pathBox6 = new QLineEdit("Option 6", topBottomWidget);

        QString buttonText = u8"\U0001F5C0";
        QFont buttonFont = QFont("Noto Color Emoji", 25);
        int buttonX = 30;
        int buttonY = 30;
        QPushButton *button1 = new QPushButton(buttonText, topBottomWidget);
        button1->setFixedSize(buttonX, buttonY);
        button1->setFont(buttonFont);
        button1->setStyleSheet(folderButtonStyle);
        QPushButton *button2 = new QPushButton(buttonText, topBottomWidget);
        button2->setFixedSize(buttonX, buttonY);
        button2->setFont(buttonFont);
        button2->setStyleSheet(folderButtonStyle);
        QPushButton *button3 = new QPushButton(buttonText, topBottomWidget);
        button3->setFixedSize(buttonX, buttonY);
        button3->setFont(buttonFont);
        button3->setStyleSheet(folderButtonStyle);
        QPushButton *button4 = new QPushButton(buttonText, topBottomWidget);
        button4->setFixedSize(buttonX, buttonY);
        button4->setFont(buttonFont);
        button4->setStyleSheet(folderButtonStyle);
        QPushButton *button5 = new QPushButton(buttonText, topBottomWidget);
        button5->setFixedSize(buttonX, buttonY);
        button5->setFont(buttonFont);
        button5->setStyleSheet(folderButtonStyle);
        QPushButton *button6 = new QPushButton(buttonText, topBottomWidget);
        button6->setFixedSize(buttonX, buttonY);
        button6->setFont(buttonFont);
        button6->setStyleSheet(folderButtonStyle);

        layoutBottom->addWidget(label1, 0, 0, 1, 6);
        layoutBottom->addWidget(label2, 1, 0, 1, 1);
        layoutBottom->addWidget(label3, 2, 0, 1, 1);
        layoutBottom->addWidget(label4, 3, 0, 1, 1);
        layoutBottom->addWidget(label5, 4, 0, 1, 1);
        layoutBottom->addWidget(label6, 5, 0, 1, 1);
        layoutBottom->addWidget(label7, 6, 0, 1, 1);

        layoutBottom->addWidget(pathBox1, 1, 1, 1, 4);
        layoutBottom->addWidget(pathBox2, 2, 1, 1, 4);
        layoutBottom->addWidget(pathBox3, 3, 1, 1, 4);
        layoutBottom->addWidget(pathBox4, 4, 1, 1, 4);
        layoutBottom->addWidget(pathBox5, 5, 1, 1, 4);
        layoutBottom->addWidget(pathBox6, 6, 1, 1, 4);

        layoutBottom->addWidget(button1, 1, 5, 1, 1);
        layoutBottom->addWidget(button2, 2, 5, 1, 1);
        layoutBottom->addWidget(button3, 3, 5, 1, 1);
        layoutBottom->addWidget(button4, 4, 5, 1, 1);
        layoutBottom->addWidget(button5, 5, 5, 1, 1);
        layoutBottom->addWidget(button6, 6, 5, 1, 1);

        layoutBottom->setRowStretch(7, 1);
    }
};
