#pragma once

#include <QString>

namespace ButtonStyle
{

inline const QString Info = R"(
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

inline const QString Active = R"(
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

inline const QString Error = R"(
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

inline const QString Inactive = R"(
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

inline const QString Disabled = R"(
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

}  // namespace ButtonStyle
