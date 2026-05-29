#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QKeyEvent>
#include <QMessageBox>
#include <QQueue>
#include <QRegularExpression>
#include <QSlider>
#include <QTextEdit>

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

    connect(ui->startGameButton, &QPushButton::clicked, this, [this]() {
        refreshLevelSelectUi();
        ui->stackedWidget->setCurrentWidget(ui->mapPage);
    });

    connect(ui->settingsButton, &QPushButton::clicked, this, [this]() {
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
    });

    connect(ui->backToMenuButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
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
    });

    connect(ui->mapFightButton, &QPushButton::clicked, this, [this]() {
        startLevel(0);
    });

    connect(ui->mapEliteButton, &QPushButton::clicked, this, [this]() {
        startLevel(1);
    });

    connect(ui->mapBossButton, &QPushButton::clicked, this, [this]() {
        startLevel(2);
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

        QPushButton#startGameButton, QPushButton#settingsButton, QPushButton#exitButton {
            background: rgba(9, 12, 18, 205);
            color: #f8e4a3;
            border: 2px solid #a77225;
            border-radius: 6px;
            font-size: 18px;
            font-weight: 800;
            padding: 8px 24px;
            min-width: 210px;
            max-width: 210px;
            min-height: 48px;
            max-height: 48px;
            letter-spacing: 0px;
        }

        QPushButton#startGameButton {
            background: rgba(42, 25, 11, 205);
            border: 3px solid #f0bd48;
            color: #fff2b6;
            font-size: 19px;
        }

        QPushButton#startGameButton:hover, QPushButton#settingsButton:hover, QPushButton#exitButton:hover {
            background: rgba(18, 35, 43, 215);
            border-color: #55e3e0;
            color: #eaffff;
        }

        QPushButton#startGameButton:pressed, QPushButton#settingsButton:pressed, QPushButton#exitButton:pressed {
            background: rgba(6, 10, 16, 230);
            border-color: #f47d3c;
            color: #ffd39b;
            padding-top: 10px;
            padding-bottom: 6px;
        }

        QLabel#mapTitleLabel {
            color: #f6d878;
            font-size: 30px;
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
            background: #2b3f24;
            border: 2px solid #819552;
            border-radius: 8px;
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
            background: #d4a94b;
            color: #2b1b10;
            border: 3px solid #7a4f21;
            border-radius: 8px;
            font-size: 18px;
            font-weight: 800;
            padding: 14px;
            text-align: left;
        }

        QPushButton[levelCard="true"]:hover {
            background: #e7bd61;
            border-color: #f4d58d;
        }

        QPushButton[levelCard="locked"] {
            background: #4b4f55;
            color: #b3b3b3;
            border: 3px solid #2c3036;
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
    ui->titleLabel->hide();
    ui->menuSubtitleLabel->hide();
    QHBoxLayout *menuButtonRow = new QHBoxLayout();
    menuButtonRow->setSpacing(22);
    menuButtonRow->setContentsMargins(0, 0, 0, 0);
    menuButtonRow->addStretch();
    ui->mainMenuLayout->removeWidget(ui->startGameButton);
    ui->mainMenuLayout->removeWidget(ui->settingsButton);
    ui->mainMenuLayout->removeWidget(ui->exitButton);
    menuButtonRow->addWidget(ui->startGameButton);
    menuButtonRow->addWidget(ui->settingsButton);
    menuButtonRow->addWidget(ui->exitButton);
    menuButtonRow->addStretch();
    ui->mainMenuLayout->insertLayout(ui->mainMenuLayout->count() - 1, menuButtonRow);
    ui->mainMenuLayout->setContentsMargins(0, 0, 0, 78);
    ui->mainMenuTopSpacer->changeSize(20, 620, QSizePolicy::Minimum, QSizePolicy::Expanding);
    ui->mainMenuMiddleSpacer->changeSize(20, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
    ui->mainMenuBottomSpacer->changeSize(20, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
    ui->startGameButton->setFixedSize(210, 48);
    ui->settingsButton->setFixedSize(210, 48);
    ui->exitButton->setFixedSize(210, 48);
    ui->mainMenuLayout->invalidate();

    ui->hpLabel->setText("Mode: Explore");
    ui->deckButton->setText("Bag");
    ui->mapButton->setText("Manual");
    ui->nextChallengeButton->setText("Undo");
    ui->challengeTitleLabel->setText("Interaction");
    ui->answerLabel->setText("Blocks");
    ui->answerLineEdit->setPlaceholderText("Example: block_add1, block_mul2");
    ui->submitAnswerButton->setText("Fill");

    ui->mapFrame->setMinimumHeight(320);
    ui->mapFightButton->setProperty("levelCard", "true");
    ui->mapEliteButton->setProperty("levelCard", "true");
    ui->mapBossButton->setProperty("levelCard", "true");
    ui->mapFightButton->setMinimumSize(190, 190);
    ui->mapEliteButton->setMinimumSize(190, 190);
    ui->mapBossButton->setMinimumSize(190, 190);
    ui->mapFightButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->mapEliteButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->mapBossButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    while (QLayoutItem *item = ui->mapNodeLayout->takeAt(0)) {
        delete item;
    }
    QHBoxLayout *levelRoadLayout = new QHBoxLayout();
    levelRoadLayout->setSpacing(14);
    levelRoadLayout->setContentsMargins(24, 24, 24, 24);
    QLabel *pathOne = new QLabel("===", ui->mapFrame);
    QLabel *pathTwo = new QLabel("===", ui->mapFrame);
    pathOne->setProperty("levelPath", "true");
    pathTwo->setProperty("levelPath", "true");
    pathOne->setAlignment(Qt::AlignCenter);
    pathTwo->setAlignment(Qt::AlignCenter);
    levelRoadLayout->addWidget(ui->mapFightButton, 3);
    levelRoadLayout->addWidget(pathOne, 1);
    levelRoadLayout->addWidget(ui->mapEliteButton, 3);
    levelRoadLayout->addWidget(pathTwo, 1);
    levelRoadLayout->addWidget(ui->mapBossButton, 3);
    ui->mapNodeLayout->addLayout(levelRoadLayout);

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
    ui->mapTitleLabel->setText("Adventure Select");
    QPushButton *buttons[] = {ui->mapFightButton, ui->mapEliteButton, ui->mapBossButton};
    const QStringList names = {"Meadow Gate", "Factory Yard", "Core Tower"};
    const QStringList subtitles = {"Start point\nLearn the room flow", "Advanced route\nMore blocks and traps", "Final chamber\nBoss program awaits"};
    for (int i = 0; i < 3; ++i) {
        const bool available = i < levels.size();
        buttons[i]->setEnabled(available);
        buttons[i]->setProperty("levelCard", available ? "true" : "locked");
        buttons[i]->style()->unpolish(buttons[i]);
        buttons[i]->style()->polish(buttons[i]);
        buttons[i]->setText(available
                                ? QString("Level %1\n%2\n\n%3").arg(i + 1).arg(names.value(i)).arg(subtitles.value(i))
                                : QString("Level %1\nLocked\n\nClear previous level").arg(i + 1));
    }
    ui->mapBackButton->setText("Back");
}

void MainWindow::startLevel(int levelIndex)
{
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
        ui->challengeTextEdit->setPlainText(levels.at(currentLevelIndex).clues.value(tileId, "No clue text."));
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
        code.replace(QString("$%1$").arg(it.key()), it.value());
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
