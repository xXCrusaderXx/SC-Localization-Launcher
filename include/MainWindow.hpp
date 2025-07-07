#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrent/QtConcurrent>

#include "EnvActions/EptuEnv.hpp"
#include "EnvActions/HotfixEnv.hpp"
#include "EnvActions/LiveEnv.hpp"
#include "EnvActions/PtuEnv.hpp"
#include "EnvActions/TechPreviewEnv.hpp"

#include "GitRepoHandler/GitChecker.hpp"
#include "GitRepoHandler/GitCloner.hpp"

class MainWindow : public QMainWindow
{
    Q_OBJECT

   private:
    QLabel *labelLogo;
    QLabel *labelBigText;
    QLabel *labelStatusLive;
    QLabel *labelStatusPtu;
    QLabel *labelStatusEptu;
    QLabel *labelStatusHotfix;
    QLabel *labelStatusTechPreview;

    QPushButton *buttonUpdate;
    QPushButton *buttonUpdateVailable;

    QPushButton *buttonSettings;

    QWidget *centralWidget;
    QGridLayout *gridLayout;

    LiveEnv *live = nullptr;
    PtuEnv *ptu = nullptr;
    EptuEnv *eptu = nullptr;
    HotfixEnv *hotfix = nullptr;
    TechPreviewEnv *techPreview = nullptr;

    GitCloner *gitCloner = nullptr;
    GitChecker *gitChecker = nullptr;

   public:
    MainWindow (QWidget *parent = nullptr)
        : QMainWindow(parent)
    {
        this->setStyleSheet(R"(
            QMainWindow {
                background-image: url(:/background.png);
                background-repeat: no-repeat;
                background-position: center;
            }
        )");
        // this->setStyleSheet("background-color: #1e1e1e;");

        setWindowTitle("SC Deutsch Launcher Linux");
        setFixedSize(800, 450);

        centralWidget = new QWidget(this);

        gridLayout = new QGridLayout(centralWidget);

        setupButtons(gridLayout);

        // Logo
        labelLogo = new QLabel(this);
        labelLogo->setFixedHeight(150);
        labelLogo->setAlignment(Qt::AlignCenter);
        QPixmap logo(":/logo.png");
        if(!logo.isNull())
        {
            labelLogo->setPixmap(logo.scaled(labelLogo->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        gridLayout->addWidget(labelLogo, 0, 5, 1, 3);

        // Status-Label
        labelBigText = new QLabel("SC Deutsch Launcher Linux", this);
        labelBigText->setAlignment(Qt::AlignCenter);
        gridLayout->addWidget(labelBigText, 0, 0, 1, 4);

        labelStatusLive = new QLabel("4.0.2 LIVE (02.03.2025, 18:46)", this);
        labelStatusLive->setAlignment(Qt::AlignCenter);
        gridLayout->addWidget(labelStatusLive, 8, 0, 1, 2);

        labelStatusPtu = new QLabel("4.0.2 PTU (02.03.2025, 18:47)", this);
        labelStatusPtu->setAlignment(Qt::AlignCenter);
        gridLayout->addWidget(labelStatusPtu, 8, 2, 1, 2);

        labelStatusEptu = new QLabel("4.0.2 PTU (02.03.2025, 18:47)", this);
        labelStatusEptu->setAlignment(Qt::AlignCenter);
        gridLayout->addWidget(labelStatusEptu, 8, 4, 1, 2);

        setCentralWidget(centralWidget);

        checkGitRepo();
    }

    void setupButtons (QGridLayout *grid)
    {
        live = new LiveEnv(this, grid);
        ptu = new PtuEnv(this, grid);
        eptu = new EptuEnv(this, grid);
        hotfix = new HotfixEnv(this, grid);
        techPreview = new TechPreviewEnv(this, grid);

        buttonUpdate = new QPushButton("Update Übersetzng", this);
        buttonUpdateVailable = new QPushButton("Update Verfügbar", this);
        grid->addWidget(buttonUpdate, 7, 0, 1, 2);
        grid->addWidget(buttonUpdateVailable, 7, 2, 1, 2);

        connect(buttonUpdate, &QPushButton::clicked, this, &MainWindow::onUpdateClicked);
        connect(buttonUpdateVailable, &QPushButton::clicked, this, &MainWindow::buttonUpdateVailableClicked);

        buttonUpdate->setDisabled(true);
        buttonUpdateVailable->setDisabled(true);

        buttonUpdate->setStyleSheet(Style::UpdateButtons::ButtonInactive);
        buttonUpdateVailable->setStyleSheet(Style::UpdateButtons::ButtonInactive);
    }

    void setUpdateAvailableButton (bool upToDate)
    {
        if(upToDate)
        {
            buttonUpdateVailable->setText("Datenbank Aktuell");
            buttonUpdateVailable->setStyleSheet(Style::UpdateButtons::ButtonInfo);
            buttonUpdate->setDisabled(true);
            buttonUpdate->setStyleSheet(Style::UpdateButtons::ButtonDisabled);

            live->updateButtons();
            ptu->updateButtons();
            eptu->updateButtons();
        }
        else
        {
            buttonUpdateVailable->setText("Update Verfügbar");
            buttonUpdateVailable->setStyleSheet(Style::UpdateButtons::ButtonActiv);
            buttonUpdate->setDisabled(false);
            buttonUpdate->setStyleSheet(Style::UpdateButtons::ButtonInactive);
        }
    }

    void checkGitRepo ()
    {
        std::cout << "CheckGitRepo for Update" << std::endl;

        auto *gitChecker = new GitChecker(
            [this] (bool upToDate)
            {
                QMetaObject::invokeMethod(
                    this,
                    [this, upToDate] ()
                    {
                        std::cout << "Update verfügbar: " << std::boolalpha << upToDate << std::endl;
                        setUpdateAvailableButton(upToDate);
                    },
                    Qt::QueuedConnection);
            });

        std::thread t(&GitChecker::run, gitChecker);
        t.join();
    }

   private slots:
    void onStartClicked ()
    {
        // labelStatus->setText("Status: Spiel wird gestartet...");
        QMessageBox::information(this, "Start", "Spiel wird gestartet!");
        // labelStatus->setText("Status: Bereit");
    }

    void onUpdateClicked ()
    {
        auto *gitCloner =
            new GitCloner([this] (bool upToDate)
                          { QMetaObject::invokeMethod(this, [this, upToDate] () { setUpdateAvailableButton(upToDate); }, Qt::QueuedConnection); });

        std::thread t(&GitCloner::run, gitCloner);
        t.detach();

        buttonUpdateVailable->setText("Update Läuft...");
        buttonUpdateVailable->setStyleSheet(Style::UpdateButtons::ButtonActiv);
        buttonUpdate->setDisabled(true);
    }

    void buttonUpdateVailableClicked () {}

    void onSettingsClicked () { QMessageBox::information(this, "Einstellungen", "Einstellungen öffnen..."); }
};
