#pragma once

#include <QString>

namespace Style
{

namespace EnvButtons
{

inline const QString ButtonInfo = R"(
    QPushButton {
        background-color: rgb(81, 186, 24);
        color: white;
        font-weight: bold;
        border-radius: 4px;
        padding: 6px 12px;
    }
    QPushButton:hover {
        background-color: rgb(100, 200, 50);
    }
    QPushButton:pressed {
        background-color: rgb(60, 150, 20);
    }
)";

inline const QString ButtonActiv = R"(
    QPushButton {
        background-color: rgb(255, 165, 0);
        color: white;
        font-weight: bold;
        border-radius: 4px;
        padding: 6px 12px;
    }
    QPushButton:hover {
        background-color: rgb(255, 180, 30);
    }
    QPushButton:pressed {
        background-color: rgb(220, 140, 0);
    }
)";

inline const QString ButtonError = R"(
    QPushButton {
        background-color: rgb(192, 0, 0);
        color: white;
        font-weight: bold;
        border-radius: 4px;
        padding: 6px 12px;
    }
    QPushButton:hover {
        background-color: rgb(220, 30, 30);
    }
    QPushButton:pressed {
        background-color: rgb(150, 0, 0);
    }
)";

inline const QString ButtonInactive = R"(
    QPushButton {
        background-color: rgb(74, 74, 74);
        color: rgb(140, 140, 140);
        border: 1px solid rgb(140, 140, 140);
        border-radius: 4px;
        padding: 6px 12px;
    }
    QPushButton:hover {
        background-color: rgb(74, 74, 74);
        color: rgb(200, 200, 200);
        border: 1px solid rgb(200, 200, 200);
    }
    QPushButton:pressed {
        background-color: rgb(74, 74, 74);
    }
)";

inline const QString ButtonDisabled = R"(
    QPushButton {
        background-color: rgb(40, 40, 40);
        color: rgb(80, 80, 80);
        border: 1px solid rgb(80, 80, 80);
        border-radius: 4px;
        padding: 6px 12px;
    }
    QPushButton:hover {
        background-color: rgb(40, 40, 40);
    }
    QPushButton:pressed {
        background-color: rgb(40, 40, 40);
    }
)";

}  // namespace EnvButtons

namespace UpdateButtons
{

inline const QString ButtonInfo = R"(
    QPushButton {
        background-color: rgb(81, 186, 24);
        color: white;
        font-weight: bold;
        border-radius: 4px;
        padding: 6px 12px;
    }
    QPushButton:hover {
        background-color: rgb(100, 200, 50);
    }
    QPushButton:pressed {
        background-color: rgb(60, 150, 20);
    }
)";

inline const QString ButtonActiv = R"(
    QPushButton {
        background-color: rgb(255, 165, 0);
        color: white;
        font-weight: bold;
        border-radius: 4px;
        padding: 6px 12px;
    }
    QPushButton:hover {
        background-color: rgb(255, 180, 30);
    }
    QPushButton:pressed {
        background-color: rgb(220, 140, 0);
    }
)";

inline const QString ButtonError = R"(
    QPushButton {
        background-color: rgb(192, 0, 0);
        color: white;
        font-weight: bold;
        border-radius: 4px;
        padding: 6px 12px;
    }
    QPushButton:hover {
        background-color: rgb(220, 30, 30);
    }
    QPushButton:pressed {
        background-color: rgb(150, 0, 0);
    }
)";

inline const QString ButtonInactive = R"(
    QPushButton {
        background-color: rgb(74, 74, 74);
        color: rgb(140, 140, 140);
        border: 1px solid rgb(180, 180, 180);
        border-radius: 4px;
        padding: 6px 12px;
    }
    QPushButton:hover {
        background-color: rgb(235, 235, 235);
    }
    QPushButton:pressed {
        background-color: rgb(200, 200, 200);
    }
)";

inline const QString ButtonDisabled = R"(
    QPushButton {
        background-color: rgb(40, 40, 40);
        color: rgb(80, 80, 80);
        border: 1px solid rgb(80, 80, 80);
        border-radius: 4px;
        padding: 6px 12px;
    }
    QPushButton:hover {
        background-color: rgb(40, 40, 40);
    }
    QPushButton:pressed {
        background-color: rgb(40, 40, 40);
    }
)";
}  // namespace UpdateButtons

}  // namespace Style
