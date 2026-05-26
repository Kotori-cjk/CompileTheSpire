#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QMessageBox>
#include <QSlider>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    applyVisualStyle();
    applyDefaultSettings();
    loadChallenges();
    resetRun();
    ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);

    connect(ui->startGameButton, &QPushButton::clicked, this, [this]() {
        resetRun();
        ui->stackedWidget->setCurrentWidget(ui->gamePage);
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
        ui->stackedWidget->setCurrentWidget(ui->deckPage);
    });

    connect(ui->deckBackButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->gamePage);
    });

    connect(ui->mapButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->mapPage);
    });

    connect(ui->mapBackButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->gamePage);
    });

    connect(ui->mapFightButton, &QPushButton::clicked, this, [this]() {
        startChallenge(0);
    });

    connect(ui->mapEliteButton, &QPushButton::clicked, this, [this]() {
        startChallenge(1);
    });

    connect(ui->mapBossButton, &QPushButton::clicked, this, [this]() {
        startChallenge(2);
    });

    connect(ui->submitAnswerButton, &QPushButton::clicked, this, [this]() {
        submitAnswer();
    });

    connect(ui->answerLineEdit, &QLineEdit::returnPressed, this, [this]() {
        submitAnswer();
    });

    connect(ui->nextChallengeButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->mapPage);
    });

    connect(ui->saveSettingsButton, &QPushButton::clicked, this, [this]() {
        if (ui->windowModeComboBox->currentText() == "Fullscreen") {
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
            background: #15181d;
            color: #ece8dc;
            font-family: "Microsoft YaHei UI";
            font-size: 14px;
        }

        QLabel#titleLabel {
            color: #f4d58d;
            font-size: 34px;
            font-weight: 700;
        }

        QLabel#menuSubtitleLabel, QLabel#settingsSubtitleLabel, QLabel#combatLogLabel {
            color: #b8b2a5;
        }

        QFrame, QGroupBox, QListWidget {
            background: #20252d;
            border: 1px solid #3d4652;
            border-radius: 6px;
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
            background: #334155;
            border: 1px solid #64748b;
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
            background: #252a31;
            color: #727b86;
            border-color: #363c44;
        }

        QPushButton#nextChallengeButton {
            background: #7c2d12;
            border-color: #fb923c;
            font-weight: 700;
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

void MainWindow::resetRun()
{
    gold = 99;
    ui->deckListWidget->clear();
    for (const Challenge &challenge : challengeRepository.challenges()) {
        ui->deckListWidget->addItem(QString("%1 - %2 blank(s)")
                                        .arg(challenge.title)
                                        .arg(challenge.rules.size()));
    }
    refreshMapUi();
    startChallenge(0);
}

void MainWindow::loadChallenges()
{
    const QStringList candidatePaths = {
        QDir(QCoreApplication::applicationDirPath()).filePath("challenges.json"),
        QDir::current().filePath("challenges.json"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../challenges.json")
    };

    QString lastError;
    for (const QString &path : candidatePaths) {
        if (challengeRepository.loadFromJsonFile(path, &lastError)) {
            statusBar()->showMessage(QString("Loaded challenges: %1").arg(path), 3000);
            return;
        }
    }

    challengeRepository.loadFallbackChallenges();
    statusBar()->showMessage("Using built-in fallback challenges.", 3000);
}

void MainWindow::refreshMapUi()
{
    const QVector<Challenge> &challenges = challengeRepository.challenges();
    QPushButton *buttons[] = {ui->mapFightButton, ui->mapEliteButton, ui->mapBossButton};
    const QStringList prefixes = {"Fight", "Elite", "Boss"};

    for (int i = 0; i < 3; ++i) {
        const bool hasChallenge = i < challenges.size();
        buttons[i]->setEnabled(hasChallenge);
        buttons[i]->setText(hasChallenge
                                ? QString("%1: %2").arg(prefixes.at(i), challenges.at(i).title)
                                : QString("%1: Locked").arg(prefixes.at(i)));
    }
}

void MainWindow::startChallenge(int challengeIndex)
{
    const QVector<Challenge> &challenges = challengeRepository.challenges();
    if (challengeIndex < 0 || challengeIndex >= challenges.size()) {
        statusBar()->showMessage("No challenge exists for this node.", 2500);
        return;
    }

    currentChallengeIndex = challengeIndex;
    const Challenge &challenge = challenges.at(currentChallengeIndex);
    floor = challenge.floor;
    challengeSolved = false;
    ui->challengeTextEdit->setPlainText(challenge.prompt);
    ui->answerLineEdit->clear();
    ui->answerLineEdit->setEnabled(true);
    ui->submitAnswerButton->setEnabled(true);
    ui->combatLogLabel->setText(QString("Challenge loaded: %1. Source: %2")
                                    .arg(challenge.title, challengeRepository.sourceDescription()));
    refreshGameUi();
    ui->stackedWidget->setCurrentWidget(ui->gamePage);
    ui->answerLineEdit->setFocus();
}

void MainWindow::refreshGameUi()
{
    ui->hpLabel->setText("Mode: Code Fill");
    ui->goldLabel->setText(QString("Gold: %1").arg(gold));
    ui->floorLabel->setText(QString("Floor: %1").arg(floor));
    ui->playerHpLabel->setText(challengeSolved ? "Progress: Solved" : "Progress: Working");
    ui->playerBlockLabel->setText("Rule: correct fill defeats");
    const QVector<Challenge> &challenges = challengeRepository.challenges();
    const QString title = currentChallengeIndex >= 0 && currentChallengeIndex < challenges.size()
                              ? challenges.at(currentChallengeIndex).title
                              : "Unknown Challenge";
    ui->enemyNameLabel->setText(title);
    ui->enemyHpLabel->setText(challengeSolved ? "Status: Defeated" : "Status: Unsolved");
    ui->enemyIntentLabel->setText("Defeat condition: correct answer");
    ui->nextChallengeButton->setEnabled(challengeSolved);
}

void MainWindow::submitAnswer()
{
    if (challengeSolved) {
        ui->combatLogLabel->setText("Already solved. Open the map for the next challenge.");
        return;
    }

    const QVector<Challenge> &challenges = challengeRepository.challenges();
    if (currentChallengeIndex < 0 || currentChallengeIndex >= challenges.size()) {
        ui->combatLogLabel->setText("No active challenge.");
        return;
    }

    QString errorMessage;
    if (answersMatchChallenge(ui->answerLineEdit->text(), challenges.at(currentChallengeIndex), &errorMessage)) {
        challengeSolved = true;
        gold += 12;
        ui->answerLineEdit->setEnabled(false);
        ui->submitAnswerButton->setEnabled(false);
        ui->combatLogLabel->setText(QString("Correct. %1 defeated. +12 gold.")
                                        .arg(challenges.at(currentChallengeIndex).title));
    } else {
        ui->combatLogLabel->setText(QString("Not quite. %1").arg(errorMessage));
        ui->answerLineEdit->selectAll();
    }

    refreshGameUi();
}
