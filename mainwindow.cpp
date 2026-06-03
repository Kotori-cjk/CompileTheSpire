#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mapview.h"
#include "codeblockui.h"
#include "stagecatalog.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDrag>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSlider>
#include <QStyle>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resize(1366, 768);
    applyVisualStyle();
    applyDefaultSettings();
    buildRuntimeGameUi();
    setupMovementShortcuts();
    bgmAudioOutput = new QAudioOutput(this);
    bgmPlayer = new QMediaPlayer(this);
    bgmPlayer->setAudioOutput(bgmAudioOutput);
    updateBgmVolume();
    connect(bgmPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia && !currentBgmResource.isEmpty()) {
            bgmPlayer->setPosition(0);
            bgmPlayer->play();
        }
    });
    connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, [this]() {
        syncBgmToCurrentPage();
    });
    connect(&gameEngine, &GameEngine::levelLoaded, this, [this]() {
        syncFromEngineState();
        ui->combatLogLabel->setText("Use WASD/arrow keys or click a reachable tile.");
        refreshGameUi();
    });
    connect(&gameEngine, &GameEngine::moveCompleted, this, [this](const MoveResult &result) {
        activeMovePath.clear();
        if (!suppressNextMovePath) {
            for (const QPoint &step : result.movePath) {
                activeMovePath.append(QPoint(step.y(), step.x()));
            }
            activeMovePathIndex = activeMovePath.size();
        }
        suppressNextMovePath = false;
        syncFromEngineState();
        QString eventText = QString("Moved to %1,%2.").arg(playerColumn).arg(playerRow);
        if (result.event == "chest") {
            eventText = "Found a chest.";
        } else if (result.event == "clue") {
            eventText = "Found a clue.";
        } else if (result.event == "monster" || result.event == "boss") {
            const Monster monster = monsterByTile(result.eventId);
            const QString name = monster.nickname.isEmpty() ? monster.name : monster.nickname;
            eventText = QString("Encountered %1.").arg(name.isEmpty() ? "enemy" : name);
        } else if (result.event != "empty") {
            eventText = "Triggered an event.";
        }
        ui->combatLogLabel->setText(eventText);
        refreshGameUi();
        if (result.event == "clue") {
            QTimer::singleShot(0, this, [this, clueId = result.eventId]() {
                if (gameEngine.m_map && !gameEngine.m_map->clueRevealed(clueId)) {
                    const bool revealed = gameEngine.revealClue(clueId);
                    syncFromEngineState();
                    refreshGameUi();
                    if (revealed && currentLevelIndex >= 0 && currentLevelIndex < levels.size()) {
                        const QString clueText = levels.at(currentLevelIndex).clues.value(clueId).val.trimmed();
                        QDialog clueDialog(this);
                        clueDialog.setModal(true);
                        clueDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
                        QVBoxLayout *clueLayout = new QVBoxLayout(&clueDialog);
                        clueLayout->setContentsMargins(16, 16, 16, 16);
                        QFrame *panel = new QFrame(&clueDialog);
                        panel->setStyleSheet(
                            "QFrame { background: rgba(9, 13, 18, 245); border: 2px solid #4feaff; border-radius: 8px; }"
                            "QLabel { color: #f9f1d0; font-size: 16px; font-weight: 700; background: transparent; }"
                        );
                        QVBoxLayout *panelLayout = new QVBoxLayout(panel);
                        panelLayout->setContentsMargins(22, 18, 22, 18);
                        panelLayout->setSpacing(14);
                        QLabel *text = new QLabel(clueText.isEmpty() ? QString("Clue recorded.") : clueText, panel);
                        text->setWordWrap(true);
                        text->setAlignment(Qt::AlignCenter);
                        QPushButton *ok = new QPushButton("OK", panel);
                        ok->setMinimumSize(120, 36);
                        ok->setStyleSheet("QPushButton { color: #fff1c2; background: #34495f; border: 1px solid #8fa8c4; border-radius: 6px; }"
                                          "QPushButton:hover { background: #42617d; }");
                        connect(ok, &QPushButton::clicked, &clueDialog, &QDialog::accept);
                        panelLayout->addWidget(text);
                        panelLayout->addWidget(ok, 0, Qt::AlignCenter);
                        clueLayout->addWidget(panel);
                        clueDialog.resize(360, 160);
                        clueDialog.exec();
                    }
                }
            });
        }
    });
    connect(&gameEngine, &GameEngine::forcedMove, this, [this](const QPoint &) {
        activeMovePath.clear();
        activeMovePathIndex = 0;
        movePlaybackActive = false;
        suppressNextMovePath = false;
        syncFromEngineState();
        refreshGameUi();
    });
    connect(&gameEngine, &GameEngine::chestEntered, this, [this](const QString &chestId) {
        QTimer::singleShot(0, this, [this, chestId]() {
            handleChest(chestId);
            syncFromEngineState();
            refreshGameUi();
        });
    });
    connect(&gameEngine, &GameEngine::combatStarted, this, [this](const QString &monsterId, Combat *) {
        QTimer::singleShot(0, this, [this, monsterId]() {
            if (monsterId != "boss" && !monsterId.startsWith("monster")) {
                if (gameEngine.m_combat) {
                    gameEngine.exitCombat();
                }
                syncFromEngineState();
                refreshGameUi();
                ui->combatLogLabel->setText("Ignored a stale cleared event tile.");
                return;
            }
            if (!gameEngine.m_combat) {
                syncFromEngineState();
                refreshGameUi();
                ui->combatLogLabel->setText("Combat event was ignored because no backend combat is active.");
                return;
            }
            handleMonster(monsterId);
            syncFromEngineState();
            refreshGameUi();
        });
    });
    connect(&gameEngine, &GameEngine::combatEnded, this, [this](const CombatResult &) {
        syncFromEngineState();
        refreshGameUi();
    });
    connect(&gameEngine, &GameEngine::levelUnlocked, this, [this](int levelIndex) {
        if (levelIndex >= 0 && !newlyUnlockedStageIndexes.contains(levelIndex)) {
            newlyUnlockedStageIndexes.append(levelIndex);
        }
        refreshLevelSelectUi();
    });
    loadLevels();
    refreshLevelSelectUi();
    ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
    mainMenuButtonBar->show();
    mainMenuButtonBar->raise();
    syncBgmToCurrentPage();
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
        refreshLevelSelectUi();
        ui->stackedWidget->setCurrentWidget(ui->mapPage);
        mainMenuButtonBar->hide();
    });

    connect(ui->backToMenuButton, &QPushButton::clicked, this, [this]() {
        if (gameEngine.m_map && currentLevelIndex >= 0) {
            ui->stackedWidget->setCurrentWidget(ui->gamePage);
            mainMenuButtonBar->hide();
        } else {
            ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
            mainMenuButtonBar->show();
            positionMainMenuButtons();
        }
    });

    connect(ui->deckButton, &QPushButton::clicked, this, [this]() {
        clearDisplayedMovePath();
        showBagDialog();
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

    connect(ui->nextChallengeButton, &QPushButton::clicked, this, [this]() {
        undo();
    });

    connect(resetRunButton, &QPushButton::clicked, this, [this]() {
        resetLevel();
    });

    connect(ui->manualButton, &QPushButton::clicked, this, [this]() {
        clearDisplayedMovePath();
        showManualDialog();
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
        updateBgmVolume();
    });
    connect(ui->musicVolumeSlider, &QSlider::valueChanged, this, [this, updatePercent]() {
        updatePercent(ui->musicVolumeSlider, ui->musicVolumeValueLabel);
        updateBgmVolume();
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

void MainWindow::setupMovementShortcuts()
{
    qApp->installEventFilter(this);
    setFocusPolicy(Qt::StrongFocus);
    if (mapView) {
        mapView->setFocusPolicy(Qt::StrongFocus);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress
        && ui
        && ui->stackedWidget->currentWidget() == ui->gamePage
        && !QApplication::activeModalWidget()) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (handleGameMoveKey(keyEvent->key())) {
            return true;
        }
        switch (keyEvent->key()) {
        case Qt::Key_Z:
            undo();
            return true;
        case Qt::Key_R:
            resetLevel();
            return true;
        case Qt::Key_B:
            clearDisplayedMovePath();
            showBagDialog();
            return true;
        case Qt::Key_M:
            clearDisplayedMovePath();
            showManualDialog();
            return true;
        case Qt::Key_Escape:
            ui->stackedWidget->setCurrentWidget(ui->pausePage);
            return true;
        default:
            break;
        }
    }
    if (event->type() == QEvent::MouseButtonDblClick
        && ui
        && ui->stackedWidget->currentWidget() == ui->mapPage) {
        QPushButton *stageButton = qobject_cast<QPushButton *>(watched);
        if (stageButton && stageButton->property("levelIndex").isValid()) {
            const int levelIndex = stageButton->property("levelIndex").toInt();
            if (isLevelUnlocked(levelIndex)) {
                startLevel(levelIndex);
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::handleGameMoveKey(int key)
{
    if (ui->stackedWidget->currentWidget() == ui->gamePage) {
        switch (key) {
        case Qt::Key_W:
        case Qt::Key_Up:
            movePlayer(-1, 0);
            return true;
        case Qt::Key_S:
        case Qt::Key_Down:
            movePlayer(1, 0);
            return true;
        case Qt::Key_A:
        case Qt::Key_Left:
            movePlayer(0, -1);
            return true;
        case Qt::Key_D:
        case Qt::Key_Right:
            movePlayer(0, 1);
            return true;
        default:
            break;
        }
    }
    return false;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (!QApplication::activeModalWidget() && handleGameMoveKey(event->key())) {
        return;
    }
    if (ui->stackedWidget->currentWidget() == ui->gamePage && !QApplication::activeModalWidget()) {
        switch (event->key()) {
        case Qt::Key_Z:
            undo();
            return;
        case Qt::Key_R:
            resetLevel();
            return;
        case Qt::Key_B:
            clearDisplayedMovePath();
            showBagDialog();
            return;
        case Qt::Key_M:
            clearDisplayedMovePath();
            showManualDialog();
            return;
        case Qt::Key_Escape:
            ui->stackedWidget->setCurrentWidget(ui->pausePage);
            return;
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
            border-image: url(:/images/assets/title_cover_16_9.png) 0 0 0 0 stretch stretch;
        }

        QWidget#mapPage {
            border-image: url(:/images/assets/stage_select_bg.png) 0 0 0 0 stretch stretch;
        }

        QWidget#settingsPage {
            background-color: #15161a;
            border-image: url(:/images/assets/interface_background_fitted.png) 0 0 0 0 stretch stretch;
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

        QLabel#settingsTitleLabel {
            color: #f1d37c;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 26px;
            font-weight: 900;
            padding: 6px 22px;
            background: rgba(27, 20, 12, 220);
            border: 2px solid #d7b75f;
            border-radius: 6px;
        }

        QLabel#settingsTitleLabel {
            max-width: 420px;
        }

        QLabel#settingsSubtitleLabel {
            background: rgba(24, 24, 26, 170);
            border: 1px solid rgba(90, 90, 90, 110);
            border-radius: 4px;
            padding: 4px 12px;
        }

        QFrame[bagPanel="true"] {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(58, 43, 25, 245),
                                        stop:0.55 rgba(28, 22, 15, 245),
                                        stop:1 rgba(16, 13, 9, 248));
            border: 3px solid #090706;
            border-radius: 6px;
        }

        QFrame[bagBoard="true"] {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(48, 39, 26, 242),
                                        stop:1 rgba(20, 17, 12, 248));
            border: 4px solid #090706;
            border-radius: 4px;
        }

        QLabel[bagSection="true"] {
            color: #f2d790;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 17px;
            font-weight: 900;
            background: transparent;
            border: none;
        }

        QLabel[codeStrip="true"] {
            color: #1c1208;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 #fff0ca,
                                        stop:0.50 #d49d61,
                                        stop:1 #7a4a25);
            border: 2px solid #100b08;
            border-radius: 4px;
            padding: 6px 8px;
            font-family: "Consolas", "Cascadia Mono", "Microsoft YaHei UI";
            font-size: 13px;
            font-weight: 700;
        }

        QPushButton[bagPageButton="true"] {
            background: rgba(24, 18, 12, 230);
            border: 2px solid #d0aa59;
            border-radius: 5px;
            color: #ffe5a0;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-weight: 900;
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
                                        fx:0.38, fy:0.30,
                                        stop:0 rgba(140, 255, 255, 245),
                                        stop:0.18 rgba(40, 183, 212, 238),
                                        stop:0.54 rgba(8, 45, 64, 245),
                                        stop:1 rgba(1, 8, 14, 255));
            color: #eaffff;
            border: 4px solid rgba(80, 240, 255, 225);
            border-radius: 42px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 30px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelCard="true"]:hover {
            background: qradialgradient(cx:0.5, cy:0.45, radius:0.75,
                                        stop:0 rgba(255, 255, 255, 245),
                                        stop:0.28 rgba(91, 242, 255, 245),
                                        stop:0.68 rgba(12, 68, 90, 250),
                                        stop:1 rgba(3, 8, 14, 255));
            border-color: #c6fbff;
            color: #ffffff;
        }

        QPushButton[levelCard="selected"] {
            background: qradialgradient(cx:0.5, cy:0.43, radius:0.75,
                                        stop:0 rgba(255, 252, 188, 255),
                                        stop:0.28 rgba(71, 238, 255, 255),
                                        stop:0.62 rgba(14, 111, 143, 250),
                                        stop:1 rgba(2, 15, 24, 255));
            color: #ffffff;
            border: 5px solid #f7d75b;
            border-radius: 42px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 34px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelCard="selectedUnlocked"] {
            background: qradialgradient(cx:0.5, cy:0.43, radius:0.75,
                                        stop:0 rgba(222, 255, 226, 255),
                                        stop:0.30 rgba(74, 222, 128, 255),
                                        stop:0.66 rgba(11, 91, 53, 250),
                                        stop:1 rgba(3, 17, 13, 255));
            color: #f6fff7;
            border: 5px solid #62ffa0;
            border-radius: 42px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 34px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelCard="selectedCleared"] {
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.74,
                                        stop:0 rgba(232, 255, 220, 255),
                                        stop:0.34 rgba(101, 214, 98, 252),
                                        stop:0.68 rgba(37, 94, 36, 252),
                                        stop:1 rgba(7, 15, 10, 255));
            color: #f8fff1;
            border: 5px solid #74ff82;
            border-radius: 42px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 34px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelCard="cleared"] {
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.74,
                                        stop:0 rgba(255, 244, 166, 250),
                                        stop:0.34 rgba(194, 130, 28, 245),
                                        stop:0.68 rgba(66, 42, 14, 250),
                                        stop:1 rgba(8, 9, 12, 255));
            color: #ffe89b;
            border: 4px solid #ffca55;
            border-radius: 42px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 30px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelCard="locked"] {
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.7,
                                        stop:0 rgba(67, 69, 72, 230),
                                        stop:0.56 rgba(27, 29, 33, 248),
                                        stop:1 rgba(4, 5, 7, 255));
            color: #b6aa92;
            border: 4px solid #5b5144;
            border-radius: 42px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 28px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelNav="true"], QPushButton#levelStartButton {
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

        QPushButton[levelMode="true"] {
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.72,
                                        stop:0 rgba(88, 238, 255, 230),
                                        stop:0.38 rgba(14, 67, 86, 245),
                                        stop:1 rgba(2, 8, 14, 255));
            border: 3px solid #25d9ef;
            border-radius: 18px;
            color: #78f6ff;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 18px;
            font-weight: 900;
            padding: 6px 18px;
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
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.73,
                                        stop:0 rgba(255, 230, 255, 245),
                                        stop:0.35 rgba(166, 73, 190, 248),
                                        stop:1 rgba(29, 10, 42, 255));
            border: 3px solid #f0a2ff;
            border-radius: 18px;
            color: #fff2ff;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 18px;
            font-weight: 900;
            padding: 6px 18px;
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
            color: rgba(62, 232, 255, 245);
            background: transparent;
            border: none;
            font-size: 42px;
            font-weight: 900;
        }

        QLabel[levelBridge="locked"] {
            color: rgba(115, 128, 139, 210);
            background: transparent;
            border: none;
            font-size: 42px;
            font-weight: 900;
        }

        QLabel[levelBridge="cleared"] {
            color: rgba(255, 206, 91, 235);
            background: transparent;
            border: none;
            font-size: 42px;
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

    QList<QLabel *> settingsLabels = {
        ui->musicVolumeLabel,
        ui->sfxVolumeLabel,
        ui->windowModeLabel,
        ui->resolutionLabel,
        ui->languageLabel,
        ui->animationSpeedLabel
    };
    if (QLabel *volumeLabel = findChild<QLabel *>("volumeLabel")) {
        settingsLabels.prepend(volumeLabel);
    }
    for (QLabel *label : settingsLabels) {
        if (!label) {
            continue;
        }
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setFixedWidth(150);
    }
    if (!ui->settingsPage->findChild<QLabel *>("controlsGuideLabel")) {
        QLabel *controlsGuide = new QLabel(ui->settingsPage);
        controlsGuide->setObjectName("controlsGuideLabel");
        controlsGuide->setText("Controls: Z Undo    R Reset    B Bag    M Manual    Esc Menu");
        controlsGuide->setAlignment(Qt::AlignCenter);
        controlsGuide->setWordWrap(true);
        controlsGuide->setStyleSheet(
            "QLabel#controlsGuideLabel { color: #fff1c2; background: rgba(9, 13, 18, 205);"
            "border: 1px solid rgba(215, 176, 106, 150); border-radius: 7px; padding: 10px;"
            "font-size: 16px; font-weight: 800; }"
        );
        ui->settingsLayout->insertWidget(1, controlsGuide);
    }

    ui->hpLabel->setText("Mode: Explore");
    ui->deckButton->setText("Bag");
    ui->manualButton->setText("Manual");
    ui->challengeTitleLabel->setText("Interaction");
    ui->answerLabel->setText("Blocks");
    ui->answerLineEdit->setPlaceholderText("Example: block_add1, block_mul2");
    ui->submitAnswerButton->setText("Fill");
    ui->challengeTextEdit->setContextMenuPolicy(Qt::NoContextMenu);
    ui->challengeTextEdit->setTextInteractionFlags(Qt::NoTextInteraction);
    ui->challengeFrame->hide();
    ui->mapButton->hide();
    ui->combatLogLabel->show();
    ui->nextChallengeButton->setText("Undo");
    ui->nextChallengeButton->hide();
    ui->nextChallengeButton->setEnabled(false);
    ui->pauseButton->setText("Menu");
    ui->pauseTitleLabel->setText("Menu");
    ui->resumeButton->setText("Return");
    ui->pauseMainMenuButton->setText("Exit");
    ui->bottomBarLayout->setStretch(0, 0);
    ui->bottomBarLayout->setStretch(1, 0);
    ui->bottomBarLayout->setStretch(2, 0);
    ui->bottomBarLayout->setStretch(3, 1);
    ui->bottomBarLayout->setStretch(4, 0);

    ui->mapTitleLabel->hide();
    ui->mapLayout->setContentsMargins(28, 22, 28, 20);
    ui->mapLayout->setSpacing(4);
    ui->mapFrame->setMinimumHeight(520);
    ui->mapBackButton->setProperty("levelNav", "true");
    ui->mapBackButton->setMinimumSize(190, 52);
    ui->mapFightButton->setProperty("levelCard", "true");
    ui->mapEliteButton->setProperty("levelCard", "true");
    ui->mapBossButton->setProperty("levelCard", "true");
    while (QLayoutItem *item = ui->mapNodeLayout->takeAt(0)) {
        delete item;
    }
    ui->mapNodeLayout->setContentsMargins(92, 24, 92, 18);
    ui->mapNodeLayout->setSpacing(8);

    QHBoxLayout *modeLayout = new QHBoxLayout();
    modeLayout->setSpacing(150);
    QPushButton *normalButton = new QPushButton("NORMAL", ui->mapFrame);
    QPushButton *exButton = new QPushButton("EX", ui->mapFrame);
    normalButton->setFixedSize(132, 58);
    exButton->setFixedSize(132, 58);
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
    pageLabel->setFixedHeight(48);
    ui->mapNodeLayout->addWidget(pageLabel);

    QHBoxLayout *stageLayout = new QHBoxLayout();
    stageLayout->setSpacing(38);
    QPushButton *prevButton = new QPushButton("<", ui->mapFrame);
    QPushButton *nextButton = new QPushButton(">", ui->mapFrame);
    prevButton->setFixedSize(70, 70);
    nextButton->setFixedSize(70, 70);
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
    ui->mapFightButton->setFixedSize(84, 84);
    ui->mapEliteButton->setFixedSize(84, 84);
    ui->mapBossButton->setFixedSize(84, 84);

    QWidget *nodeCanvas = new QWidget(ui->mapFrame);
    nodeCanvas->setObjectName("stageNodeCanvas");
    nodeCanvas->setFixedSize(650, 250);
    nodeCanvas->setStyleSheet("QWidget#stageNodeCanvas { background: transparent; border: none; }");
    QLabel *bridge01 = new QLabel(nodeCanvas);
    QLabel *bridge12 = new QLabel(nodeCanvas);
    bridge01->setObjectName("stageBridge0");
    bridge12->setObjectName("stageBridge1");
    bridge01->setProperty("levelBridge", "true");
    bridge12->setProperty("levelBridge", "true");
    bridge01->setAlignment(Qt::AlignCenter);
    bridge12->setAlignment(Qt::AlignCenter);
    bridge01->setPixmap(QPixmap(":/images/assets/stage_bridge_up.png"));
    bridge12->setPixmap(QPixmap(":/images/assets/stage_bridge_up2.png"));
    bridge01->setScaledContents(true);
    bridge12->setScaledContents(true);
    bridge01->setGeometry(158, 112, 250, 108);
    bridge12->setGeometry(366, 38, 250, 108);
    ui->mapFightButton->setParent(nodeCanvas);
    ui->mapEliteButton->setParent(nodeCanvas);
    ui->mapBossButton->setParent(nodeCanvas);
    ui->mapFightButton->setGeometry(92, 160, 84, 84);
    ui->mapEliteButton->setGeometry(300, 92, 84, 84);
    ui->mapBossButton->setGeometry(512, 18, 84, 84);
    ui->mapFightButton->installEventFilter(this);
    ui->mapEliteButton->installEventFilter(this);
    ui->mapBossButton->installEventFilter(this);
    bridge01->lower();
    bridge12->lower();
    bridge01->setStyleSheet("background: transparent; border: none;");
    bridge12->setStyleSheet("background: transparent; border: none;");
    ui->mapFightButton->show();
    ui->mapEliteButton->show();
    ui->mapBossButton->show();
    stageLayout->addWidget(prevButton, 0, Qt::AlignVCenter);
    stageLayout->addStretch(1);
    stageLayout->addWidget(nodeCanvas, 0, Qt::AlignCenter);
    stageLayout->addStretch(1);
    stageLayout->addWidget(nextButton, 0, Qt::AlignVCenter);
    ui->mapNodeLayout->addLayout(stageLayout, 1);

    QPushButton *levelStartButton = new QPushButton("START", ui->mapFrame);
    levelStartButton->setObjectName("levelStartButton");
    levelStartButton->setFixedSize(280, 50);
    connect(levelStartButton, &QPushButton::clicked, this, [this]() {
        startLevel(selectedStageIndex);
    });
    ui->mapNodeLayout->addSpacing(4);
    ui->mapNodeLayout->addWidget(levelStartButton, 0, Qt::AlignCenter);

    clearLayout(ui->combatLayout);
    mapView = new MapView(ui->combatFrame);
    connect(mapView, &MapView::tileClicked, this, [this](int row, int column) {
        movePlayerTo(row, column);
    });
    ui->combatLayout->addWidget(mapView, 1);

    tileInfoLabel = new QLabel(ui->combatFrame);
    tileInfoLabel->hide();

    undoRunButton = new QPushButton("Undo", ui->topBarFrame);
    undoRunButton->setEnabled(false);
    connect(undoRunButton, &QPushButton::clicked, this, [this]() {
        undo();
    });
    ui->topBarLayout->insertWidget(ui->topBarLayout->count() - 1, undoRunButton);

    resetRunButton = new QPushButton("Reset", ui->topBarFrame);
    ui->topBarLayout->insertWidget(ui->topBarLayout->count() - 1, resetRunButton);

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

void MainWindow::updateMainMenuBackground()
{
}

void MainWindow::playBgm(const QString &resourcePath)
{
    if (!bgmPlayer || resourcePath.isEmpty() || currentBgmResource == resourcePath) {
        return;
    }

    const QString appRelativePath = QDir(QCoreApplication::applicationDirPath()).filePath(resourcePath);
    const QString workingRelativePath = QDir::current().filePath(resourcePath);
    QString audioPath = QFileInfo::exists(appRelativePath) ? appRelativePath : workingRelativePath;
    if (!QFileInfo::exists(audioPath)) {
        audioPath = QDir(QCoreApplication::applicationDirPath()).filePath(QString("../CompileTheSpire/%1").arg(resourcePath));
    }
    if (!QFileInfo::exists(audioPath)) {
        statusBar()->showMessage(QString("Missing BGM: %1").arg(resourcePath), 2500);
        return;
    }

    currentBgmResource = resourcePath;
    bgmPlayer->setSource(QUrl::fromLocalFile(audioPath));
    updateBgmVolume();
    bgmPlayer->play();
}

void MainWindow::syncBgmToCurrentPage()
{
    if (!ui || !ui->stackedWidget) {
        return;
    }

    QWidget *page = ui->stackedWidget->currentWidget();
    if (page == ui->mainMenuPage) {
        playBgm("assets/audio/bgm_menu.mp3");
    } else if (page == ui->mapPage) {
        playBgm("assets/audio/bgm_stage_select.mp3");
    } else if (page == ui->gamePage || page == ui->pausePage) {
        playBgm("assets/audio/bgm_game.mp3");
    } else if (page == ui->settingsPage && currentBgmResource.isEmpty()) {
        playBgm("assets/audio/bgm_menu.mp3");
    }
}

void MainWindow::updateBgmVolume()
{
    if (!bgmAudioOutput || !ui) {
        return;
    }

    const qreal master = qBound(0, ui->volumeSlider->value(), 100) / 100.0;
    const qreal music = qBound(0, ui->musicVolumeSlider->value(), 100) / 100.0;
    bgmAudioOutput->setVolume(master * music);
}
