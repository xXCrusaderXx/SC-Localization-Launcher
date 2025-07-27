#include "MainWindow.hpp"

bool MainWindow::eventFilter (QObject *obj, QEvent *event)
{
    if(obj == centralWidget())
    {
        if(event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

            // Prüfe, ob der Klick auf einem Button ist
            QWidget *clickedWidget = centralWidget()->childAt(mouseEvent->pos());
            if(clickedWidget && qobject_cast<QPushButton *>(clickedWidget))
            {
                // Klick war auf Button → keine Drag-Aktion
                return false;  // Event normal weiterverarbeiten
            }

            // Kein Button → Fenster mit systemeigenem Drag bewegen
            if(windowHandle())
            {
                windowHandle()->startSystemMove();
                return true;  // Event verarbeitet
            }
        }
    }

    // Normale Event-Verarbeitung
    return QMainWindow::eventFilter(obj, event);
}
