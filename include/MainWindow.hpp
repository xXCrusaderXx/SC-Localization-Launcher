#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>
#include <QtConcurrent/QtConcurrent>

#include "frontend/Channels/EptuChannel.hpp"
#include "frontend/Channels/HotfixChannel.hpp"
#include "frontend/Channels/LiveChannel.hpp"
#include "frontend/Channels/PtuChannel.hpp"
#include "frontend/Channels/TechPreviewChannel.hpp"
#include "frontend/MenuBar/MenuBar.hpp"
#include "frontend/Settings/Settings.hpp"
#include "frontend/Update/Update.hpp"

#include "backend/BackEnd.hpp"
#include "logging/logging.h"

#include "protocol/Protocol.hpp"

class MainWindow : public QMainWindow
{
    Q_OBJECT

   private:
    LoggerFramework::LogEx lg;
    BackEnd backEnd;
    QLabel *labelLogo;
    QLabel *labelTitel;

    QPushButton *buttonSettings;

    MenuBar *menuBar = nullptr;
    LiveChannel *live = nullptr;
    PtuChannel *ptu = nullptr;
    EptuChannel *eptu = nullptr;
    HotfixChannel *hotfix = nullptr;
    TechPreviewChannel *techPreview = nullptr;

    Settings *settings = nullptr;
    UpdateButton *update = nullptr;

   protected:
    bool eventFilter (QObject *obj, QEvent *event) override;

   public:
    MainWindow (QWidget *parent = nullptr)
        : QMainWindow(parent)
        , lg("MainWindow")
    {
        this->setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
        setWindowTitle("::MainWindow()");
        setFixedSize(800, 430);

        this->setStyleSheet(R"(
    QMainWindow {
        background-image: url(:/background2_main.png);
        background-repeat: no-repeat;
        background-position: center;
    }
)");

        QWidget *central = new QWidget(this);
        setCentralWidget(central);
        centralWidget()->installEventFilter(this);
        QGridLayout *mainGrid = new QGridLayout(central);
        central->setLayout(mainGrid);

        QWidget *topMenuBar = new QWidget(central);
        topMenuBar->setMinimumSize(1, 2);
        mainGrid->addWidget(topMenuBar, 0, 0, 1, 2);
        QGridLayout *layoutMenuBar = new QGridLayout(topMenuBar);
        topMenuBar->setLayout(layoutMenuBar);
        menuBar = new MenuBar(this, layoutMenuBar);

        QWidget *top = new QWidget(central);
        top->setMinimumSize(1, 2);
        mainGrid->addWidget(top, 1, 0, 1, 2);
        QGridLayout *layoutTop = new QGridLayout(top);
        top->setLayout(layoutTop);
        setupTopWidged(top, layoutTop);

        QWidget *left = new QWidget(central);
        mainGrid->addWidget(left, 2, 0, 1, 1);
        QGridLayout *layoutLeft = new QGridLayout(left);
        left->setLayout(layoutLeft);
        setupChannelWidged(left, layoutLeft);

        QWidget *right = new QWidget(central);
        mainGrid->addWidget(right, 2, 1, 1, 1);
        QGridLayout *layoutRight = new QGridLayout(right);
        right->setLayout(layoutRight);

        setupLogoWidged(right, layoutRight);

        QWidget *bottom = new QWidget(central);
        bottom->setMinimumSize(2, 0);
        mainGrid->addWidget(bottom, 3, 0, 1, 2);
        QGridLayout *layoutBottom = new QGridLayout(bottom);
        bottom->setLayout(layoutBottom);

        QLabel *label1 = new QLabel(bottom);
        label1->setText("LIVE");
        layoutBottom->addWidget(label1, 0, 0, 1, 2);

        QLabel *label2 = new QLabel(bottom);
        label2->setText("PTU");
        layoutBottom->addWidget(label2, 0, 2, 1, 2);

        QLabel *label3 = new QLabel(bottom);
        label3->setText("EPTU");
        layoutBottom->addWidget(label3, 0, 4, 1, 2);

        layoutBottom->setRowStretch(1, 1);

        backEnd.setGuiCallback([this] (const nlohmann::json &msg) { this->ProcessResponse(msg); });
        backEnd.initGui();
    }

    void setupTopWidged (QWidget *widged, QGridLayout *grid)
    {
        LOG_DEBUG(lg) << "::setupTopWidged()";
        labelTitel = new QLabel(this);
        labelTitel->setAlignment(Qt::AlignLeft);
        labelTitel->setStyleSheet(R"(
    QLabel {
        font-size: 28px;
        font-weight: bold;
        color: white;
        margin: 0px;
        padding: 0px;
    }
)");

        QString htmlText = R"(
    <div style="font-size:28px; font-weight:bold; line-height: 1.0;">
        SC Deutsch Launcher<br>
        <span style="font-size:13px; font-weight:normal; line-height: 1.0;">
            Offizielle Community Edition
        </span>
    </div>
)";
        labelTitel->setText(htmlText);
        grid->addWidget(labelTitel, 0, 0, 1, 2);

        settings = new Settings(this, grid);
        settings->setUpdateBackendCallback([this] (const std::string &key, const nlohmann::json &msg) { backEnd.processMassage(msg); });
        backEnd.setGuiCallbacks(settings->getName(), [this] (const nlohmann::json &msg) { settings->updateStatus(msg); });
    };

    void setupLogoWidged (QWidget *widged, QGridLayout *grid)
    {
        LOG_DEBUG(lg) << "::setupLogoWidged()";
        labelLogo = new QLabel(this);
        labelLogo->setFixedSize(100, 100);
        labelLogo->setAlignment(Qt::AlignBottom | Qt::AlignRight);
        QPixmap logo(":/logo.png");
        if(!logo.isNull())
        {
            labelLogo->setPixmap(logo.scaled(labelLogo->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        grid->setColumnStretch(0, 1);
        grid->setRowStretch(0, 1);
        grid->addWidget(labelLogo, 1, 1);
    }

    void setupChannelWidged (QWidget *widged, QGridLayout *grid)
    {
        LOG_DEBUG(lg) << "::setupChannelWidged()";

        live = new LiveChannel(this, grid);
        ptu = new PtuChannel(this, grid);
        eptu = new EptuChannel(this, grid);
        hotfix = new HotfixChannel(this, grid);
        techPreview = new TechPreviewChannel(this, grid);
        update = new UpdateButton(this, grid);

        live->setUpdateBackendCallback([this] (const nlohmann::json &msg) { backEnd.processMassage(msg); });
        ptu->setUpdateBackendCallback([this] (const nlohmann::json &msg) { backEnd.processMassage(msg); });
        eptu->setUpdateBackendCallback([this] (const nlohmann::json &msg) { backEnd.processMassage(msg); });
        hotfix->setUpdateBackendCallback([this] (const nlohmann::json &msg) { backEnd.processMassage(msg); });
        techPreview->setUpdateBackendCallback([this] (const nlohmann::json &msg) { backEnd.processMassage(msg); });
        update->setUpdateBackendCallback([this] (const nlohmann::json &msg) { backEnd.processMassage(msg); });

        backEnd.setGuiCallbacks(live->getName(), [this] (const nlohmann::json &msg) { live->updateStatus(msg); });
        backEnd.setGuiCallbacks(ptu->getName(), [this] (const nlohmann::json &msg) { ptu->updateStatus(msg); });
        backEnd.setGuiCallbacks(eptu->getName(), [this] (const nlohmann::json &msg) { eptu->updateStatus(msg); });
        backEnd.setGuiCallbacks(hotfix->getName(), [this] (const nlohmann::json &msg) { hotfix->updateStatus(msg); });
        backEnd.setGuiCallbacks(techPreview->getName(), [this] (const nlohmann::json &msg) { techPreview->updateStatus(msg); });
        backEnd.setGuiCallbacks(update->getName(), [this] (const nlohmann::json &msg) { update->updateStatus(msg); });
    }

    void ProcessResponse (const nlohmann::json &msg)
    {
        LOG_DEBUG(lg) << "ProcessResponse() - START";
        LOG_DEBUG(lg) << "ProcessResponse() - RESPONSE: " << msg.dump(4);

        Protocol::Massage response(msg);
        if(response.moduleExist("LIVE")) live->updateStatus(response.getModuleNode("LIVE"));
        if(response.moduleExist("PTU")) ptu->updateStatus(response.getModuleNode("PTU"));
        if(response.moduleExist("EPTU")) eptu->updateStatus(response.getModuleNode("EPTU"));
        if(response.moduleExist("HOTFIX")) hotfix->updateStatus(response.getModuleNode("HOTFIX"));
        if(response.moduleExist("TECH-PREVIEW")) techPreview->updateStatus(response.getModuleNode("TECH-PREVIEW"));
        if(response.moduleExist("UPDATE")) update->updateStatus(response.getModuleNode("UPDATE"));
        if(response.moduleExist("SETTINGS")) settings->updateStatus(response.getModuleNode("SETTINGS"));

        LOG_DEBUG(lg) << "ProcessResponse() - FINISHED";
    }
};
