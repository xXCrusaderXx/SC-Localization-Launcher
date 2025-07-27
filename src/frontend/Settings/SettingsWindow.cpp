#include "frontend/Settings/SettingsWindow.hpp"

bool SettingsWindow::eventFilter (QObject *obj, QEvent *event)
{
    if(obj == this)
    {
        if(event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

            QWidget *clickedWidget = childAt(mouseEvent->pos());
            if(clickedWidget && qobject_cast<QPushButton *>(clickedWidget))
            {
                return false;
            }

            if(windowHandle())
            {
                windowHandle()->startSystemMove();
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}
