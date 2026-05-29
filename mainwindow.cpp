#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QKeyEvent>
#include <QMessageBox>
#include <QQueue>
#include <QRegularExpression>
#include <QSlider>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>

namespace {
void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        if (QLayout *childLayout = item->layout()) {
            clearLayout(childLayout);
            delete childLayout;
        }
        delete item;
    }
}

QStringList fallbackLevelPaths()
{
    return {
        QDir(QCoreApplication::applicationDirPath()).filePath("levels"),
        QDir::current().filePath("levels"),
        QCoreApplication::applicationDirPath(),
        QDir::currentPath()
    };
}

struct StageCard
{
    int levelIndex;
    bool isEx;
    QString title;
    QString subtitle;
    QString hint;
};

QVector<StageCard> stageCatalog()
{
    return {
        {0, false, "Stage 1", "Pointer Gate", "Warm-up path. Learn movement, chests, and code blocks."},
        {1, false, "Stage 2", "Stack Corridor", "A compact dungeon route with stricter block choices."},
        {2, false, "Stage 3", "Array Depths", "Beware the bounds. The boss gate starts watching."},
        {3, false, "Stage 4", "Heap Bridge", "A branching route that asks the player to plan block order."},
        {4, false, "Stage 5", "Lambda Atrium", "Introduce slightly longer fill chains and optional clues."},
        {5, false, "Stage 6", "Graph Lift", "Unlocked after the lower bridge is cleared."},
        {6, false, "Stage 7", "Bit Forge", "A late normal route for denser C++ syntax puzzles."},
        {7, false, "Stage 8", "Memory Crown", "The final normal ascent before the spire core."},
        {8, false, "Stage 9", "Compile Throne", "Normal boss placeholder for the full chapter arc."},
        {9, true, "EX-1", "Template Rift", "Extra challenge route. Designed for later hard levels."},
        {10, true, "EX-2", "Iterator Vault", "Optional branch with denser clue and chest logic."},
        {11, true, "EX-3", "Undefined Core", "A harder fill route with fewer visible hints."},
        {12, true, "EX-4", "Const Labyrinth", "Requires prior EX proof before entry."},
        {13, true, "EX-5", "Overload Gate", "A special branch for advanced C++ mechanics."},
        {14, true, "EX-6", "Shadow Linker", "A locked extension for future JSON content."},
        {15, true, "EX-7", "Metaprogram Pit", "Late EX placeholder with strict topology."},
        {16, true, "EX-8", "Undefined Peak", "The final optional climb."},
        {17, true, "EX-9", "Core Dump", "EX boss placeholder for the chapter finale."}
    };
}

constexpr int stagesPerPage = 3;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    applyVisualStyle();
    applyDefaultSettings();
    buildRuntimeGameUi();
    loadLevels();
    refreshLevelSelectUi();
    ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
    mainMenuButtonBar->show();
    mainMenuButtonBar->raise();
    QTimer::singleShot(0, this, &MainWindow::positionMainMenuButtons);

    connect(ui->startGameButton, &QPushButton::clicked, this, [this]() {
        mainMenuButtonBar->hide();
        refreshLevelSelectUi();
        ui->stackedWidget->setCurrentWidget(ui->mapPage);
    });

    connect(ui->settingsButton, &QPushButton::clicked, this, [this]() {
        mainMenuButtonBar->hide();
        ui->stackedWidget->setCurrentWidget(ui->settingsPage);
    });

    connect(ui->pauseButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pausePage);
    });

    connect(ui->resumeButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->gamePage);
    });

    connect(ui->pauseSettingsButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->settingsPage);
    });

    connect(ui->pauseMainMenuButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
        mainMenuButtonBar->show();
        positionMainMenuButtons();
    });

    connect(ui->backToMenuButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
        mainMenuButtonBar->show();
        positionMainMenuButtons();
    });

    connect(ui->deckButton, &QPushButton::clicked, this, [this]() {
        refreshBagPage();
        ui->stackedWidget->setCurrentWidget(ui->deckPage);
    });

    connect(ui->deckBackButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->gamePage);
    });

    connect(ui->mapBackButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
        mainMenuButtonBar->show();
        positionMainMenuButtons();
    });

    connect(ui->mapFightButton, &QPushButton::clicked, this, [this]() {
        selectStage(ui->mapFightButton->property("levelIndex").toInt());
    });

    connect(ui->mapEliteButton, &QPushButton::clicked, this, [this]() {
        selectStage(ui->mapEliteButton->property("levelIndex").toInt());
    });

    connect(ui->mapBossButton, &QPushButton::clicked, this, [this]() {
        selectStage(ui->mapBossButton->property("levelIndex").toInt());
    });

    connect(ui->submitAnswerButton, &QPushButton::clicked, this, [this]() {
        submitFill();
    });

    connect(ui->answerLineEdit, &QLineEdit::returnPressed, this, [this]() {
        submitFill();
    });

    connect(ui->nextChallengeButton, &QPushButton::clicked, this, [this]() {
        undo();
    });

    connect(resetRunButton, &QPushButton::clicked, this, [this]() {
        resetLevel();
    });

    connect(handbookButton, &QPushButton::clicked, this, [this]() {
        refreshManualPage();
        ui->stackedWidget->setCurrentWidget(ui->deckPage);
    });

    connect(ui->saveSettingsButton, &QPushButton::clicked, this, [this]() {
        if (ui->windowModeComboBox->currentText() == "Fullscreen" || ui->fullscreenCheckBox->isChecked()) {
            showFullScreen();
        } else {
            showNormal();
        }
        statusBar()->showMessage("Settings applied.", 2500);
    });

    connect(ui->resetSettingsButton, &QPushButton::clicked, this, [this]() {
        applyDefaultSettings();
        statusBar()->showMessage("Settings restored to defaults.", 2500);
    });

    const auto updatePercent = [](QSlider *slider, QLabel *label) {
        label->setText(QString("%1%").arg(slider->value()));
    };

    connect(ui->volumeSlider, &QSlider::valueChanged, this, [this, updatePercent]() {
        updatePercent(ui->volumeSlider, ui->volumeValueLabel);
    });
    connect(ui->musicVolumeSlider, &QSlider::valueChanged, this, [this, updatePercent]() {
        updatePercent(ui->musicVolumeSlider, ui->musicVolumeValueLabel);
    });
    connect(ui->sfxVolumeSlider, &QSlider::valueChanged, this, [this, updatePercent]() {
        updatePercent(ui->sfxVolumeSlider, ui->sfxVolumeValueLabel);
    });

    connect(ui->exitButton, &QPushButton::clicked, this, &MainWindow::close);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (ui->stackedWidget->currentWidget() == ui->gamePage) {
        switch (event->key()) {
        case Qt::Key_W:
        case Qt::Key_Up:
            movePlayer(-1, 0);
            return;
        case Qt::Key_S:
        case Qt::Key_Down:
            movePlayer(1, 0);
            return;
        case Qt::Key_A:
        case Qt::Key_Left:
            movePlayer(0, -1);
            return;
        case Qt::Key_D:
        case Qt::Key_Right:
            movePlayer(0, 1);
            return;
        case Qt::Key_E:
        case Qt::Key_Return:
            interactWithCurrentTile();
            return;
        case Qt::Key_Z:
            if (event->modifiers() & Qt::ControlModifier) {
                undo();
                return;
            }
            break;
        default:
            break;
        }
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    positionMainMenuButtons();
}

void MainWindow::applyDefaultSettings()
{
    ui->volumeSlider->setValue(80);
    ui->musicVolumeSlider->setValue(70);
    ui->sfxVolumeSlider->setValue(75);
    ui->windowModeComboBox->setCurrentIndex(0);
    ui->resolutionComboBox->setCurrentIndex(0);
    ui->fullscreenCheckBox->setChecked(false);
    ui->languageComboBox->setCurrentIndex(0);
    ui->animationSpeedComboBox->setCurrentIndex(0);
    ui->tutorialCheckBox->setChecked(true);
    ui->volumeValueLabel->setText("80%");
    ui->musicVolumeValueLabel->setText("70%");
    ui->sfxVolumeValueLabel->setText("75%");
}

void MainWindow::applyVisualStyle()
{
    setStyleSheet(R"(
        QMainWindow, QWidget {
            background: #121316;
            color: #f1ead8;
            font-family: "Microsoft YaHei UI";
            font-size: 14px;
        }

        QLabel#titleLabel {
            color: #f6d878;
            font-size: 34px;
            font-weight: 700;
        }

        QWidget#mainMenuPage {
            border-image: url(:/images/assets/cover.png) 0 0 0 0 stretch stretch;
        }

        QWidget#mapPage {
            border-image: url(:/images/assets/stage_select_bg.png) 0 0 0 0 stretch stretch;
        }

        QPushButton#startGameButton, QPushButton#settingsButton, QPushButton#exitButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(58, 38, 14, 225),
                                        stop:0.48 rgba(20, 24, 30, 225),
                                        stop:1 rgba(5, 8, 13, 238));
            color: #fff0b7;
            border: 3px solid #d59731;
            border-radius: 8px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 20px;
            font-weight: 900;
            padding: 8px 24px;
            min-width: 220px;
            max-width: 220px;
            min-height: 52px;
            max-height: 52px;
            letter-spacing: 0px;
        }

        QPushButton#startGameButton {
            border-color: #ffd25b;
            color: #fff6c8;
        }

        QPushButton#startGameButton:hover, QPushButton#settingsButton:hover, QPushButton#exitButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(55, 91, 94, 235),
                                        stop:0.5 rgba(16, 39, 48, 238),
                                        stop:1 rgba(7, 10, 16, 245));
            border-color: #57edf6;
            color: #ffffff;
        }

        QPushButton#startGameButton:pressed, QPushButton#settingsButton:pressed, QPushButton#exitButton:pressed {
            background: rgba(7, 9, 14, 245);
            border-color: #ff8f3e;
            color: #ffdca4;
            padding-top: 11px;
            padding-bottom: 5px;
        }

        QLabel#mapTitleLabel {
            color: #f6d878;
            font-size: 24px;
            font-weight: 700;
        }

        QLabel#menuSubtitleLabel, QLabel#settingsSubtitleLabel, QLabel#combatLogLabel, QLabel#tileInfoLabel {
            color: #cfc6ad;
        }

        QFrame, QGroupBox, QListWidget {
            background: #242832;
            border: 1px solid #454b59;
            border-radius: 6px;
        }

        QFrame#mapFrame {
            background: transparent;
            border: none;
            border-radius: 0px;
        }

        QFrame#combatFrame {
            background: #191d24;
            border: 2px solid #3d4652;
            border-radius: 6px;
        }

        QFrame#challengeFrame, QFrame#bottomBarFrame, QFrame#topBarFrame {
            background: #20242c;
            border: 1px solid #515968;
        }

        QGroupBox {
            margin-top: 12px;
            padding-top: 14px;
            font-weight: 700;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: #f4d58d;
        }

        QPushButton {
            background: #344151;
            border: 1px solid #708197;
            border-radius: 5px;
            padding: 7px 14px;
            color: #f8fafc;
        }

        QPushButton:hover {
            background: #3f516b;
        }

        QPushButton:pressed {
            background: #263244;
        }

        QPushButton:disabled {
            background: #24272d;
            color: #777c83;
            border-color: #363b44;
        }

        QPushButton[levelCard="true"] {
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.72,
                                        fx:0.42, fy:0.32,
                                        stop:0 rgba(62, 66, 72, 240),
                                        stop:0.55 rgba(23, 25, 29, 245),
                                        stop:1 rgba(5, 8, 13, 250));
            color: #e9dcc0;
            border: 5px solid #5e5549;
            border-radius: 56px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 34px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelCard="true"]:hover {
            background: qradialgradient(cx:0.5, cy:0.45, radius:0.75,
                                        stop:0 rgba(38, 91, 104, 245),
                                        stop:0.65 rgba(10, 31, 42, 250),
                                        stop:1 rgba(3, 8, 14, 255));
            border-color: #87f8ff;
            color: #ffffff;
        }

        QPushButton[levelCard="selected"] {
            background: qradialgradient(cx:0.5, cy:0.43, radius:0.75,
                                        stop:0 rgba(38, 145, 165, 245),
                                        stop:0.48 rgba(15, 80, 101, 245),
                                        stop:1 rgba(3, 12, 20, 255));
            color: #ffffff;
            border: 6px solid #35eaff;
            border-radius: 62px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 40px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelCard="cleared"] {
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.74,
                                        stop:0 rgba(117, 92, 35, 245),
                                        stop:0.58 rgba(43, 32, 18, 250),
                                        stop:1 rgba(8, 9, 12, 255));
            color: #ffe89b;
            border: 5px solid #ffca55;
            border-radius: 56px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 34px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelCard="locked"] {
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.7,
                                        stop:0 rgba(49, 50, 52, 240),
                                        stop:1 rgba(16, 17, 20, 250));
            color: #b9ac91;
            border: 5px solid #5b5144;
            border-radius: 56px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 32px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelNav="true"], QPushButton[levelMode="true"], QPushButton#levelStartButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(16, 42, 53, 235),
                                        stop:0.5 rgba(7, 18, 27, 238),
                                        stop:1 rgba(3, 8, 14, 245));
            border: 3px solid #1fd5ec;
            border-radius: 8px;
            color: #65f5ff;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 20px;
            font-weight: 900;
            padding: 7px 18px;
        }

        QPushButton[levelNav="true"]:hover, QPushButton[levelMode="true"]:hover, QPushButton#levelStartButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(47, 91, 96, 245),
                                        stop:1 rgba(8, 25, 35, 250));
            border-color: #c2fbff;
            color: #ffffff;
        }

        QPushButton[levelNav="true"]:pressed, QPushButton[levelMode="true"]:pressed, QPushButton#levelStartButton:pressed {
            background: rgba(5, 10, 16, 250);
            border-color: #ffba52;
            color: #ffe9a8;
        }

        QPushButton#levelStartButton {
            font-size: 34px;
            letter-spacing: 0px;
        }

        QPushButton[levelMode="active"] {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(108, 47, 122, 245),
                                        stop:1 rgba(31, 14, 44, 250));
            border-color: #f0a2ff;
            color: #fff2ff;
        }

        QLabel#levelPageLabel {
            background: rgba(16, 22, 32, 218);
            border: 2px solid rgba(213, 151, 49, 210);
            border-radius: 8px;
            color: #62f3ff;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 24px;
            font-weight: 900;
            padding: 8px 20px;
            letter-spacing: 0px;
        }

        QLabel[levelBridge="true"] {
            color: rgba(34, 219, 247, 225);
            font-size: 34px;
            font-weight: 900;
        }

        QLabel[levelBridge="locked"] {
            color: rgba(73, 78, 86, 175);
            font-size: 34px;
            font-weight: 900;
        }

        QLabel[levelBridge="cleared"] {
            color: rgba(255, 206, 91, 235);
            font-size: 34px;
            font-weight: 900;
        }

        QLabel[levelPath="true"] {
            color: #f6d878;
            font-size: 28px;
            font-weight: 800;
        }

        QPushButton[tileRole="player"] {
            background: #f6d878;
            color: #15181d;
            border-color: #fff4c2;
            font-weight: 700;
        }

        QPushButton[tileRole="monster"] {
            background: #842626;
            border-color: #ff776d;
            color: #fff1eb;
        }

        QPushButton[tileRole="chest"] {
            background: #946318;
            border-color: #ffd15a;
            color: #fff3c2;
        }

        QPushButton[tileRole="clue"] {
            background: #1e6550;
            border-color: #45d39f;
            color: #daf7eb;
        }

        QPushButton[tileRole="boss"] {
            background: #5f2d7d;
            border-color: #d99cff;
            color: #f5e8ff;
        }

        QPushButton[tileRole="wall"] {
            background: #30343b;
            border-color: #171a1f;
            color: #737987;
        }

        QPushButton[tileRole="floor"] {
            background: #323947;
            border-color: #576172;
            color: #aeb7c8;
        }

        QPushButton[tileRole="start"] {
            background: #245f3d;
            border-color: #6de09c;
            color: #dfffe8;
        }

        QPushButton[tileRole="cleared"] {
            background: #2d3641;
            border-color: #596474;
            color: #97a3b5;
        }

        QPushButton[mapTile="true"] {
            border-width: 2px;
            border-radius: 4px;
            font-size: 13px;
            font-weight: 700;
            padding: 4px;
        }

        QTextEdit, QLineEdit {
            background: #111827;
            border: 1px solid #3d4652;
            border-radius: 5px;
            color: #f9f1d0;
            padding: 8px;
        }

        QComboBox, QSlider, QLineEdit {
            min-height: 28px;
        }

        QListWidget {
            padding: 10px;
        }
    )");
}

void MainWindow::buildRuntimeGameUi()
{
    if (QLayout *oldMenuLayout = ui->mainMenuPage->layout()) {
        delete oldMenuLayout;
    }
    ui->titleLabel->hide();
    ui->menuSubtitleLabel->hide();
    ui->mainMenuBackgroundLabel->hide();
    mainMenuButtonBar = new QWidget(ui->mainMenuPage);
    mainMenuButtonBar->setObjectName("mainMenuButtonBar");
    mainMenuButtonBar->setStyleSheet("QWidget#mainMenuButtonBar { background: transparent; border: none; }");
    QHBoxLayout *menuButtonRow = new QHBoxLayout(mainMenuButtonBar);
    menuButtonRow->setSpacing(40);
    menuButtonRow->setContentsMargins(0, 0, 0, 0);
    ui->startGameButton->setParent(mainMenuButtonBar);
    ui->settingsButton->setParent(mainMenuButtonBar);
    ui->exitButton->setParent(mainMenuButtonBar);
    menuButtonRow->addWidget(ui->startGameButton);
    menuButtonRow->addWidget(ui->settingsButton);
    menuButtonRow->addWidget(ui->exitButton);
    ui->startGameButton->setFixedSize(220, 52);
    ui->settingsButton->setFixedSize(220, 52);
    ui->exitButton->setFixedSize(220, 52);
    mainMenuButtonBar->show();
    mainMenuButtonBar->raise();
    positionMainMenuButtons();
    QTimer::singleShot(0, this, &MainWindow::positionMainMenuButtons);

    ui->hpLabel->setText("Mode: Explore");
    ui->deckButton->setText("Bag");
    ui->mapButton->setText("Manual");
    ui->nextChallengeButton->setText("Undo");
    ui->challengeTitleLabel->setText("Interaction");
    ui->answerLabel->setText("Blocks");
    ui->answerLineEdit->setPlaceholderText("Example: block_add1, block_mul2");
    ui->submitAnswerButton->setText("Fill");

    ui->mapTitleLabel->hide();
    ui->mapLayout->setContentsMargins(28, 22, 28, 20);
    ui->mapLayout->setSpacing(4);
    ui->mapFrame->setMinimumHeight(520);
    ui->mapBackButton->setProperty("levelNav", "true");
    ui->mapBackButton->setMinimumSize(190, 52);
    ui->mapFightButton->setProperty("levelCard", "true");
    ui->mapEliteButton->setProperty("levelCard", "true");
    ui->mapBossButton->setProperty("levelCard", "true");
    ui->mapFightButton->setFixedSize(116, 116);
    ui->mapEliteButton->setFixedSize(108, 108);
    ui->mapBossButton->setFixedSize(108, 108);

    while (QLayoutItem *item = ui->mapNodeLayout->takeAt(0)) {
        delete item;
    }
    ui->mapNodeLayout->setContentsMargins(82, 28, 82, 26);
    ui->mapNodeLayout->setSpacing(12);

    QHBoxLayout *modeLayout = new QHBoxLayout();
    modeLayout->setSpacing(76);
    QPushButton *normalButton = new QPushButton("NORMAL", ui->mapFrame);
    QPushButton *exButton = new QPushButton("EX", ui->mapFrame);
    normalButton->setFixedSize(150, 54);
    exButton->setFixedSize(120, 54);
    normalButton->setProperty("levelMode", "active");
    exButton->setProperty("levelMode", "true");
    connect(normalButton, &QPushButton::clicked, this, [this]() {
        setLevelSelectMode(false);
    });
    connect(exButton, &QPushButton::clicked, this, [this]() {
        setLevelSelectMode(true);
    });
    normalButton->setObjectName("normalStageButton");
    exButton->setObjectName("exStageButton");
    modeLayout->addStretch();
    modeLayout->addWidget(normalButton);
    modeLayout->addWidget(exButton);
    modeLayout->addStretch();
    ui->mapNodeLayout->addLayout(modeLayout);

    QLabel *pageLabel = new QLabel("SELECT STAGE", ui->mapFrame);
    pageLabel->setObjectName("levelPageLabel");
    pageLabel->setAlignment(Qt::AlignCenter);
    ui->mapNodeLayout->addWidget(pageLabel);

    QHBoxLayout *stageLayout = new QHBoxLayout();
    stageLayout->setSpacing(28);
    QPushButton *prevButton = new QPushButton("<", ui->mapFrame);
    QPushButton *nextButton = new QPushButton(">", ui->mapFrame);
    prevButton->setFixedSize(74, 58);
    nextButton->setFixedSize(74, 58);
    prevButton->setProperty("levelNav", "true");
    nextButton->setProperty("levelNav", "true");
    connect(prevButton, &QPushButton::clicked, this, [this]() {
        changeLevelPage(-1);
    });
    connect(nextButton, &QPushButton::clicked, this, [this]() {
        changeLevelPage(1);
    });
    prevButton->setObjectName("prevStageButton");
    nextButton->setObjectName("nextStageButton");
    ui->mapFightButton->setObjectName("stageNodeButton0");
    ui->mapEliteButton->setObjectName("stageNodeButton1");
    ui->mapBossButton->setObjectName("stageNodeButton2");
    QGridLayout *nodeGrid = new QGridLayout();
    nodeGrid->setHorizontalSpacing(76);
    nodeGrid->setVerticalSpacing(26);
    nodeGrid->setContentsMargins(70, 18, 70, 6);
    QLabel *bridge01 = new QLabel("/////", ui->mapFrame);
    QLabel *bridge12 = new QLabel("/////", ui->mapFrame);
    bridge01->setObjectName("stageBridge0");
    bridge12->setObjectName("stageBridge1");
    bridge01->setProperty("levelBridge", "true");
    bridge12->setProperty("levelBridge", "true");
    bridge01->setAlignment(Qt::AlignCenter);
    bridge12->setAlignment(Qt::AlignCenter);
    nodeGrid->addWidget(ui->mapBossButton, 0, 4, Qt::AlignCenter);
    nodeGrid->addWidget(bridge12, 1, 3, Qt::AlignCenter);
    nodeGrid->addWidget(ui->mapEliteButton, 2, 2, Qt::AlignCenter);
    nodeGrid->addWidget(bridge01, 3, 1, Qt::AlignCenter);
    nodeGrid->addWidget(ui->mapFightButton, 4, 0, Qt::AlignCenter);
    stageLayout->addWidget(prevButton, 0, Qt::AlignVCenter);
    stageLayout->addStretch(1);
    stageLayout->addLayout(nodeGrid, 3);
    stageLayout->addStretch(1);
    stageLayout->addWidget(nextButton, 0, Qt::AlignVCenter);
    ui->mapNodeLayout->addLayout(stageLayout, 1);

    QPushButton *levelStartButton = new QPushButton("START", ui->mapFrame);
    levelStartButton->setObjectName("levelStartButton");
    levelStartButton->setFixedSize(350, 64);
    connect(levelStartButton, &QPushButton::clicked, this, [this]() {
        startLevel(selectedStageIndex);
    });
    ui->mapNodeLayout->addWidget(levelStartButton, 0, Qt::AlignCenter);

    clearLayout(ui->combatLayout);
    QWidget *gridHost = new QWidget(ui->combatFrame);
    worldGridLayout = new QGridLayout(gridHost);
    worldGridLayout->setSpacing(5);
    worldGridLayout->setContentsMargins(18, 18, 18, 18);
    ui->combatLayout->addWidget(gridHost, 3);

    QFrame *sideFrame = new QFrame(ui->combatFrame);
    sideFrame->setObjectName("sideFrame");
    QVBoxLayout *sideLayout = new QVBoxLayout(sideFrame);
    QLabel *sideTitle = new QLabel("Tile Info", sideFrame);
    QFont sideFont = sideTitle->font();
    sideFont.setPointSize(16);
    sideFont.setBold(true);
    sideTitle->setFont(sideFont);
    tileInfoLabel = new QLabel("Choose a level to enter the map.", sideFrame);
    tileInfoLabel->setObjectName("tileInfoLabel");
    tileInfoLabel->setWordWrap(true);
    QPushButton *interactButton = new QPushButton("Interact", sideFrame);
    connect(interactButton, &QPushButton::clicked, this, [this]() {
        interactWithCurrentTile();
    });
    sideLayout->addWidget(sideTitle);
    sideLayout->addWidget(tileInfoLabel, 1);
    sideLayout->addWidget(interactButton);
    ui->combatLayout->addWidget(sideFrame, 2);

    resetRunButton = new QPushButton("Reset", ui->topBarFrame);
    ui->topBarLayout->insertWidget(ui->topBarLayout->count() - 1, resetRunButton);

    handbookButton = new QPushButton("Manual", ui->bottomBarFrame);
    ui->bottomBarLayout->insertWidget(2, handbookButton);
    ui->mapButton->hide();
}

void MainWindow::positionMainMenuButtons()
{
    if (!mainMenuButtonBar) {
        return;
    }

    const int buttonWidth = 220;
    const int gap = 40;
    const int width = 3 * buttonWidth + 2 * gap;
    const int height = 60;
    const int x = qMax(0, (ui->mainMenuPage->width() - width) / 2);
    const int y = qMax(0, ui->mainMenuPage->height() - height - 6);
    mainMenuButtonBar->setGeometry(x, y, width, height);
    mainMenuButtonBar->raise();
}

void MainWindow::loadLevels()
{
    QStringList errors;
    for (const QString &path : fallbackLevelPaths()) {
        QVector<LevelData> loaded = LoadDirectory(path, &errors);
        if (!loaded.isEmpty()) {
            levels = loaded;
            statusBar()->showMessage(QString("Loaded %1 level(s) from %2").arg(levels.size()).arg(path), 3000);
            return;
        }
    }

    LevelData single;
    QString error;
    const QString examplePath = QDir::current().filePath("example.json");
    if (single.LoadFromJson(examplePath, &error)) {
        levels.append(single);
        statusBar()->showMessage("Loaded example.json.", 3000);
        return;
    }

    statusBar()->showMessage("No valid level JSON was found.", 4000);
}

void MainWindow::refreshLevelSelectUi()
{
    const QVector<StageCard> allStages = stageCatalog();
    QVector<StageCard> visibleStages;
    for (const StageCard &stage : allStages) {
        if (stage.isEx == showingExLevels) {
            visibleStages.append(stage);
        }
    }

    const int pageCount = qMax(1, (visibleStages.size() + stagesPerPage - 1) / stagesPerPage);
    currentLevelSelectPage = qBound(0, currentLevelSelectPage, pageCount - 1);
    const int firstIndex = currentLevelSelectPage * stagesPerPage;

    ui->mapTitleLabel->setText(showingExLevels ? "EX Stage Select" : "Select Stage");
    QPushButton *buttons[] = {ui->mapFightButton, ui->mapEliteButton, ui->mapBossButton};

    QLabel *pageLabel = ui->mapFrame->findChild<QLabel *>("levelPageLabel");
    QPushButton *prevButton = ui->mapFrame->findChild<QPushButton *>("prevStageButton");
    QPushButton *nextButton = ui->mapFrame->findChild<QPushButton *>("nextStageButton");
    QPushButton *normalButton = ui->mapFrame->findChild<QPushButton *>("normalStageButton");
    QPushButton *exButton = ui->mapFrame->findChild<QPushButton *>("exStageButton");
    QPushButton *startButton = ui->mapFrame->findChild<QPushButton *>("levelStartButton");
    QLabel *bridges[] = {
        ui->mapFrame->findChild<QLabel *>("stageBridge0"),
        ui->mapFrame->findChild<QLabel *>("stageBridge1")
    };

    if (pageLabel) {
        pageLabel->setText(QString("%1 PAGE %2 / %3")
                               .arg(showingExLevels ? "EX STAGE" : "SELECT STAGE")
                               .arg(currentLevelSelectPage + 1)
                               .arg(pageCount));
    }
    if (prevButton) {
        prevButton->setEnabled(currentLevelSelectPage > 0);
    }
    if (nextButton) {
        nextButton->setEnabled(currentLevelSelectPage + 1 < pageCount);
    }
    if (normalButton && exButton) {
        normalButton->setProperty("levelMode", showingExLevels ? "true" : "active");
        exButton->setProperty("levelMode", showingExLevels ? "active" : "true");
        normalButton->style()->unpolish(normalButton);
        normalButton->style()->polish(normalButton);
        exButton->style()->unpolish(exButton);
        exButton->style()->polish(exButton);
    }

    if (selectedStageIndex < 0 || !isLevelUnlocked(selectedStageIndex)) {
        selectedStageIndex = -1;
        for (int i = 0; i < stagesPerPage; ++i) {
            const int stageIndex = firstIndex + i;
            if (stageIndex < visibleStages.size() && isLevelUnlocked(visibleStages.at(stageIndex).levelIndex)) {
                selectedStageIndex = visibleStages.at(stageIndex).levelIndex;
                break;
            }
        }
    }
    bool selectedStageVisible = false;
    for (int i = 0; i < 3; ++i) {
        const int stageIndex = firstIndex + i;
        const bool hasStage = stageIndex < visibleStages.size();
        const StageCard stage = hasStage ? visibleStages.at(stageIndex) : StageCard{-1, showingExLevels, "Empty", "", ""};
        const bool unlocked = hasStage && isLevelUnlocked(stage.levelIndex);
        const bool completed = hasStage && completedStageIndexes.contains(stage.levelIndex);
        buttons[i]->setEnabled(hasStage);
        buttons[i]->setProperty("levelIndex", stage.levelIndex);
        const bool selected = hasStage && stage.levelIndex == selectedStageIndex;
        buttons[i]->setProperty("levelCard", selected ? "selected" : (completed ? "cleared" : (unlocked ? "true" : "locked")));
        buttons[i]->style()->unpolish(buttons[i]);
        buttons[i]->style()->polish(buttons[i]);
        buttons[i]->setText(hasStage
                                ? (stage.isEx ? QString("EX\n%1").arg(stageIndex + 1)
                                              : QString::number(stageIndex + 1))
                                : "-");
        if (selected) {
            selectedStageVisible = true;
        }
    }
    for (int i = 0; i < 2; ++i) {
        if (!bridges[i]) {
            continue;
        }
        const int fromIndex = firstIndex + i;
        const int toIndex = firstIndex + i + 1;
        QString state = "locked";
        if (toIndex < visibleStages.size()) {
            const int fromLevel = visibleStages.at(fromIndex).levelIndex;
            const int toLevel = visibleStages.at(toIndex).levelIndex;
            state = completedStageIndexes.contains(fromLevel) ? "cleared" : (isLevelUnlocked(toLevel) ? "true" : "locked");
        }
        bridges[i]->setProperty("levelBridge", state);
        bridges[i]->style()->unpolish(bridges[i]);
        bridges[i]->style()->polish(bridges[i]);
    }
    if (!selectedStageVisible && selectedStageIndex >= 0) {
        selectedStageIndex = -1;
        refreshLevelSelectUi();
        return;
    }
    if (startButton) {
        startButton->setEnabled(selectedStageIndex >= 0 && selectedStageIndex < levels.size() && isLevelUnlocked(selectedStageIndex));
    }
    ui->mapBackButton->setText("Back");
}

bool MainWindow::isLevelUnlocked(int levelIndex) const
{
    const QVector<StageCard> allStages = stageCatalog();
    for (int i = 0; i < allStages.size(); ++i) {
        if (allStages.at(i).levelIndex != levelIndex) {
            continue;
        }
        for (int previous = i - 1; previous >= 0; --previous) {
            if (allStages.at(previous).isEx != allStages.at(i).isEx) {
                break;
            }
            return completedStageIndexes.contains(allStages.at(previous).levelIndex);
        }
        return true;
    }
    return false;
}

void MainWindow::setLevelSelectMode(bool exMode)
{
    showingExLevels = exMode;
    currentLevelSelectPage = 0;
    selectedStageIndex = -1;
    refreshLevelSelectUi();
}

void MainWindow::changeLevelPage(int delta)
{
    currentLevelSelectPage += delta;
    selectedStageIndex = -1;
    refreshLevelSelectUi();
}

void MainWindow::selectStage(int levelIndex)
{
    if (!isLevelUnlocked(levelIndex)) {
        statusBar()->showMessage("Clear the previous connected stage first.", 2500);
        return;
    }
    selectedStageIndex = levelIndex;
    refreshLevelSelectUi();
}

void MainWindow::startLevel(int levelIndex)
{
    if (!isLevelUnlocked(levelIndex)) {
        statusBar()->showMessage("This stage is locked by the path order.", 2500);
        return;
    }
    if (levelIndex < 0 || levelIndex >= levels.size()) {
        statusBar()->showMessage("This level is not available yet.", 2500);
        return;
    }

    currentLevelIndex = levelIndex;
    resetLevel();
    ui->stackedWidget->setCurrentWidget(ui->gamePage);
}

void MainWindow::resetLevel()
{
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    playerRow = 0;
    playerColumn = 0;
    for (int row = 0; row < level.mapGrid.size(); ++row) {
        for (int column = 0; column < level.mapGrid.at(row).size(); ++column) {
            if (level.mapGrid.at(row).at(column) == "start") {
                playerRow = row;
                playerColumn = column;
            }
        }
    }

    bagBlocks.clear();
    openedChests.clear();
    defeatedMonsters.clear();
    seenMonsters.clear();
    history.clear();
    ui->combatLogLabel->setText("Use WASD/arrow keys or click a reachable tile. Press E to interact.");
    refreshGameUi();
}

void MainWindow::pushUndoState()
{
    GameSnapshot snapshot;
    snapshot.row = playerRow;
    snapshot.column = playerColumn;
    snapshot.bagBlocks = bagBlocks;
    snapshot.openedChests = openedChests;
    snapshot.defeatedMonsters = defeatedMonsters;
    snapshot.seenMonsters = seenMonsters;
    history.append(snapshot);
}

void MainWindow::undo()
{
    if (history.isEmpty()) {
        ui->combatLogLabel->setText("Nothing to undo yet.");
        return;
    }

    const GameSnapshot snapshot = history.takeLast();
    playerRow = snapshot.row;
    playerColumn = snapshot.column;
    bagBlocks = snapshot.bagBlocks;
    openedChests = snapshot.openedChests;
    defeatedMonsters = snapshot.defeatedMonsters;
    seenMonsters = snapshot.seenMonsters;
    ui->combatLogLabel->setText("Undo restored the previous state.");
    refreshGameUi();
}

void MainWindow::refreshGameUi()
{
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    ui->hpLabel->setText(QString("Level: %1").arg(currentLevelIndex + 1));
    ui->goldLabel->setText(QString("Bag: %1/%2").arg(bagBlocks.size()).arg(level.bagSize));
    ui->floorLabel->setText(QString("Pos: %1,%2").arg(playerColumn).arg(playerRow));
    ui->nextChallengeButton->setEnabled(!history.isEmpty());
    refreshMapGrid();
    refreshSidePanel();
}

void MainWindow::refreshMapGrid()
{
    if (!worldGridLayout || currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    clearLayout(worldGridLayout);
    mapButtons.clear();
    const LevelData &level = levels.at(currentLevelIndex);

    for (int row = 0; row < level.mapGrid.size(); ++row) {
        QVector<QPushButton *> rowButtons;
        for (int column = 0; column < level.mapGrid.at(row).size(); ++column) {
            const QString tileId = level.mapGrid.at(row).at(column);
            QPushButton *button = new QPushButton(tileId == "#" ? "#" : " ", ui->combatFrame);
            button->setProperty("mapTile", "true");
            button->setMinimumSize(76, 66);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            button->setEnabled(tileId != "#");
            button->setText(describeTile(tileId));
            if (row == playerRow && column == playerColumn) {
                button->setText("@\n" + describeTile(tileId));
                button->setProperty("tileRole", "player");
            } else if (tileId.startsWith("monster")) {
                button->setProperty("tileRole", defeatedMonsters.contains(tileId) ? "cleared" : "monster");
            } else if (tileId.startsWith("chest")) {
                button->setProperty("tileRole", openedChests.contains(tileId) ? "cleared" : "chest");
            } else if (tileId.startsWith("clue")) {
                button->setProperty("tileRole", "clue");
            } else if (tileId == "boss") {
                button->setProperty("tileRole", defeatedMonsters.contains(tileId) ? "cleared" : "boss");
            } else if (tileId == "#") {
                button->setProperty("tileRole", "wall");
            } else if (tileId == "start") {
                button->setProperty("tileRole", "start");
            } else {
                button->setProperty("tileRole", "floor");
            }
            connect(button, &QPushButton::clicked, this, [this, row, column]() {
                movePlayerTo(row, column);
            });
            worldGridLayout->addWidget(button, row, column);
            rowButtons.append(button);
        }
        mapButtons.append(rowButtons);
    }
}

void MainWindow::refreshSidePanel()
{
    const QString tileId = tileAt(playerRow, playerColumn);
    tileInfoLabel->setText(describeTile(tileId));
    ui->challengeTitleLabel->setText("Interaction");
    ui->answerLineEdit->setEnabled(false);
    ui->submitAnswerButton->setEnabled(false);

    if (tileId.startsWith("monster") || tileId == "boss") {
        const Monster monster = monsterByTile(tileId);
        seenMonsters.insert(tileId);
        ui->challengeTitleLabel->setText(QString("Fill %1").arg(monster.nickname.isEmpty() ? monster.name : monster.nickname));
        ui->challengeTextEdit->setPlainText(renderMonsterCode(monster));
        ui->answerLineEdit->setEnabled(!defeatedMonsters.contains(tileId));
        ui->submitAnswerButton->setEnabled(!defeatedMonsters.contains(tileId));
    } else if (tileId.startsWith("chest")) {
        const Chest chest = levels.at(currentLevelIndex).chests.value(tileId);
        QStringList lines;
        lines << QString("Chest: %1").arg(tileId)
              << QString("Forced pick: %1").arg(chest.forcedPick ? "yes" : "no")
              << QString("Repeat: %1").arg(chest.repeat ? "yes" : "no")
              << "Blocks:";
        for (const CodeBlock &block : chest.blocks) {
            lines << QString("  %1 = %2").arg(block.blockId, block.code);
        }
        ui->challengeTextEdit->setPlainText(lines.join('\n'));
    } else if (tileId.startsWith("clue")) {
        const Clue clue = levels.at(currentLevelIndex).clues.value(tileId);
        ui->challengeTextEdit->setPlainText(clue.val.isEmpty() ? "No clue text." : clue.val);
    } else {
        ui->challengeTextEdit->setPlainText("Move onto a chest, clue, monster, or boss tile and interact.");
    }
}

void MainWindow::refreshBagPage()
{
    ui->deckTitleLabel->setText("Bag");
    ui->deckListWidget->clear();
    if (bagBlocks.isEmpty()) {
        ui->deckListWidget->addItem("Empty. Open chests to collect code blocks.");
        return;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    for (const QString &blockId : bagBlocks) {
        QString code = blockId;
        for (const Chest &chest : level.chests) {
            if (chest.blocks.contains(blockId)) {
                code = chest.blocks.value(blockId).code;
                break;
            }
        }
        ui->deckListWidget->addItem(QString("%1    %2").arg(blockId, code));
    }
}

void MainWindow::refreshManualPage()
{
    ui->deckTitleLabel->setText("Monster Manual");
    ui->deckListWidget->clear();
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    for (const QString &monsterId : level.monsters.keys()) {
        const bool seen = seenMonsters.contains(monsterId) || defeatedMonsters.contains(monsterId);
        const Monster monster = level.monsters.value(monsterId);
        ui->deckListWidget->addItem(seen
                                        ? QString("%1 | %2 | %3").arg(monsterId, monster.nickname, monster.type)
                                        : QString("%1 | display(hidden)").arg(monsterId));
    }
    ui->deckListWidget->addItem(defeatedMonsters.contains("boss")
                                    ? QString("boss | %1 | defeated").arg(level.boss.nickname)
                                    : "boss | display(hidden)");
}

void MainWindow::movePlayer(int rowDelta, int columnDelta)
{
    const int nextRow = playerRow + rowDelta;
    const int nextColumn = playerColumn + columnDelta;
    if (!canEnter(nextRow, nextColumn)) {
        ui->combatLogLabel->setText("Blocked.");
        return;
    }

    pushUndoState();
    playerRow = nextRow;
    playerColumn = nextColumn;
    ui->combatLogLabel->setText(QString("Moved to %1,%2.").arg(playerColumn).arg(playerRow));
    refreshGameUi();
}

void MainWindow::movePlayerTo(int targetRow, int targetColumn)
{
    if (!canEnter(targetRow, targetColumn)) {
        ui->combatLogLabel->setText("That tile is blocked.");
        return;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    QVector<QVector<bool>> visited(level.mapGrid.size());
    QVector<QVector<QPoint>> parent(level.mapGrid.size());
    for (int row = 0; row < level.mapGrid.size(); ++row) {
        visited[row] = QVector<bool>(level.mapGrid.at(row).size(), false);
        parent[row] = QVector<QPoint>(level.mapGrid.at(row).size(), QPoint(-1, -1));
    }

    QQueue<QPoint> queue;
    queue.enqueue(QPoint(playerColumn, playerRow));
    visited[playerRow][playerColumn] = true;
    const QPoint directions[] = {QPoint(1, 0), QPoint(-1, 0), QPoint(0, 1), QPoint(0, -1)};
    while (!queue.isEmpty()) {
        const QPoint current = queue.dequeue();
        if (current.y() == targetRow && current.x() == targetColumn) {
            break;
        }
        for (const QPoint &direction : directions) {
            const int nextColumn = current.x() + direction.x();
            const int nextRow = current.y() + direction.y();
            if (canEnter(nextRow, nextColumn) && !visited[nextRow][nextColumn]) {
                visited[nextRow][nextColumn] = true;
                parent[nextRow][nextColumn] = current;
                queue.enqueue(QPoint(nextColumn, nextRow));
            }
        }
    }

    if (!visited[targetRow][targetColumn]) {
        ui->combatLogLabel->setText("No path to that tile.");
        return;
    }

    pushUndoState();
    playerRow = targetRow;
    playerColumn = targetColumn;
    ui->combatLogLabel->setText("Auto-path moved the player. Animation hook can be added here later.");
    refreshGameUi();
}

bool MainWindow::canEnter(int row, int column) const
{
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return false;
    }
    const LevelData &level = levels.at(currentLevelIndex);
    return row >= 0
           && row < level.mapGrid.size()
           && column >= 0
           && column < level.mapGrid.at(row).size()
           && level.mapGrid.at(row).at(column) != "#";
}

QString MainWindow::tileAt(int row, int column) const
{
    if (!canEnter(row, column)) {
        return "#";
    }
    return levels.at(currentLevelIndex).mapGrid.at(row).at(column);
}

QString MainWindow::describeTile(const QString &tileId) const
{
    if (tileId == "#") {
        return "Wall";
    }
    if (tileId == ".") {
        return "Room";
    }
    if (tileId == "start") {
        return "Spawn";
    }
    if (tileId == "boss") {
        return defeatedMonsters.contains("boss") ? "Cleared\nBoss" : "Boss\nGate";
    }
    if (tileId.startsWith("monster")) {
        return defeatedMonsters.contains(tileId) ? "Cleared\nFight" : "Enemy\n" + tileId.mid(QString("monster_").size());
    }
    if (tileId.startsWith("chest")) {
        return openedChests.contains(tileId) ? "Opened\nChest" : "Chest\n" + tileId.mid(QString("chest_").size());
    }
    if (tileId.startsWith("clue")) {
        return "Clue\n" + tileId.mid(QString("clue_").size());
    }
    return tileId;
}

void MainWindow::interactWithCurrentTile()
{
    const QString tileId = tileAt(playerRow, playerColumn);
    if (tileId.startsWith("chest")) {
        handleChest(tileId);
    } else if (tileId.startsWith("monster") || tileId == "boss") {
        handleMonster(tileId);
    } else if (tileId.startsWith("clue")) {
        ui->combatLogLabel->setText("Clue opened in the interaction panel.");
    } else {
        ui->combatLogLabel->setText("Nothing to interact with here.");
    }
    refreshGameUi();
}

void MainWindow::handleChest(const QString &chestId)
{
    const Chest chest = levels.at(currentLevelIndex).chests.value(chestId);
    if (openedChests.contains(chestId) && !chest.repeat) {
        ui->combatLogLabel->setText("This chest is already empty.");
        return;
    }

    pushUndoState();
    QStringList picked;
    for (const CodeBlock &block : chest.blocks) {
        if (bagBlocks.contains(block.blockId)) {
            continue;
        }
        if (bagBlocks.size() >= levels.at(currentLevelIndex).bagSize) {
            break;
        }
        bagBlocks.append(block.blockId);
        picked.append(block.blockId);
    }

    if (!chest.repeat || picked.isEmpty()) {
        openedChests.insert(chestId);
    }
    ui->combatLogLabel->setText(picked.isEmpty()
                                    ? "No new block was picked."
                                    : QString("Picked: %1").arg(picked.join(", ")));
}

void MainWindow::handleMonster(const QString &monsterId)
{
    seenMonsters.insert(monsterId);
    ui->combatLogLabel->setText(defeatedMonsters.contains(monsterId)
                                    ? "This enemy is already defeated."
                                    : "Fill the blanks with block ids from your bag.");
}

void MainWindow::submitFill()
{
    const QString tileId = tileAt(playerRow, playerColumn);
    if (!(tileId.startsWith("monster") || tileId == "boss")) {
        ui->combatLogLabel->setText("Stand on a monster or boss tile first.");
        return;
    }
    if (defeatedMonsters.contains(tileId)) {
        ui->combatLogLabel->setText("Already defeated.");
        return;
    }

    const Monster monster = monsterByTile(tileId);
    const QStringList blocks = splitAnswerBlocks(ui->answerLineEdit->text());
    if (blocks.size() != monster.spaces.size()) {
        ui->combatLogLabel->setText(QString("Need %1 block(s), got %2.").arg(monster.spaces.size()).arg(blocks.size()));
        return;
    }

    for (int i = 0; i < blocks.size(); ++i) {
        if (!bagBlocks.contains(blocks.at(i)) && tileId != "boss") {
            ui->combatLogLabel->setText(QString("%1 is not in your bag.").arg(blocks.at(i)));
            return;
        }
        if (!blockMatchesSpace(blocks.at(i), monster.spaces.at(i))) {
            ui->combatLogLabel->setText(QString("%1 does not match %2.").arg(blocks.at(i), monster.spaces.at(i).spaceId));
            return;
        }
    }

    pushUndoState();
    defeatedMonsters.insert(tileId);
    seenMonsters.insert(tileId);
    if (tileId == "boss") {
        completedStageIndexes.insert(currentLevelIndex);
    }
    ui->answerLineEdit->clear();
    ui->combatLogLabel->setText(tileId == "boss" ? levels.at(currentLevelIndex).endText : "Correct fill. Enemy defeated.");
    refreshGameUi();
}

QString MainWindow::renderMonsterCode(const Monster &monster) const
{
    QString code = monster.codeTemplate;
    for (const Space &space : monster.spaces) {
        code.replace(QString("$%1$").arg(space.spaceId), QString("[%1]").arg(space.spaceId));
    }
    for (auto it = levels.at(currentLevelIndex).clues.constBegin(); it != levels.at(currentLevelIndex).clues.constEnd(); ++it) {
        code.replace(QString("$%1$").arg(it.key()), it.value().val);
    }
    return code;
}

bool MainWindow::blockMatchesSpace(const QString &blockId, const Space &space) const
{
    for (const QString &rule : space.values) {
        if (space.type == "prefix" && blockId.startsWith(rule)) {
            return true;
        }
        if (space.type == "find" && blockId.contains(rule)) {
            return true;
        }
        if (space.type == "regex") {
            const QRegularExpression regex(rule);
            if (regex.isValid() && regex.match(blockId).hasMatch()) {
                return true;
            }
        }
        if ((space.type == "match" || space.type.isEmpty()) && blockId == rule) {
            return true;
        }
    }
    return false;
}

QStringList MainWindow::splitAnswerBlocks(const QString &text) const
{
    QString normalized = text;
    normalized.replace(';', ',');
    QStringList parts = normalized.split(',', Qt::SkipEmptyParts);
    for (QString &part : parts) {
        part = part.trimmed();
    }
    return parts;
}

Monster MainWindow::monsterByTile(const QString &tileId) const
{
    if (tileId == "boss") {
        return levels.at(currentLevelIndex).boss;
    }
    return levels.at(currentLevelIndex).monsters.value(tileId);
}
