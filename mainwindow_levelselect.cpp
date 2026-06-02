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

void MainWindow::loadLevels()
{
    QStringList errors;
    for (const QString &path : fallbackLevelPaths()) {
        Q_UNUSED(errors);
        gameEngine.gameInit(path);
        if (!gameEngine.levels.isEmpty()) {
            levels = gameEngine.levels;
            statusBar()->showMessage(QString("Loaded %1 level(s) from %2").arg(levels.size()).arg(path), 3000);
            return;
        }
    }

    LevelData single;
    QString error;
    const QString examplePath = QDir::current().filePath("example.json");
    if (single.LoadFromJson(examplePath, &error)) {
        levels.append(single);
        gameEngine.levels = levels;
        gameEngine.m_save.Init(levels.size());
        gameEngine.m_save.Load(levels.size());
        statusBar()->showMessage("Loaded example.json.", 3000);
        return;
    }

    levels.append(createPreviewLevel());
    gameEngine.levels = levels;
    gameEngine.m_save.Init(levels.size());
    gameEngine.m_save.Load(levels.size());
    statusBar()->showMessage("No valid level JSON was found. Loaded preview map for UI testing.", 5000);
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
        const bool completed = hasStage && isLevelCleared(stage.levelIndex);
        buttons[i]->setEnabled(hasStage);
        buttons[i]->setProperty("levelIndex", stage.levelIndex);
        const bool selected = hasStage && stage.levelIndex == selectedStageIndex;
        QString cardState = "locked";
        if (unlocked && completed) {
            cardState = selected ? "selectedCleared" : "cleared";
        } else if (unlocked) {
            cardState = selected ? "selectedUnlocked" : "true";
        }
        buttons[i]->setProperty("levelCard", cardState);
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
            state = isLevelCleared(fromLevel) ? "cleared" : (isLevelUnlocked(toLevel) ? "true" : "locked");
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
        startButton->setEnabled(selectedStageIndex >= 0 && !levels.isEmpty() && isLevelUnlocked(selectedStageIndex));
    }
    ui->mapBackButton->setText("Back");
}

bool MainWindow::isLevelUnlocked(int levelIndex) const
{
    if (levelIndex == 0 && !showingExLevels) {
        return true;
    }

    if (levelIndex >= 0 && levelIndex < levels.size()) {
        for (const LevelMeta &meta : const_cast<GameEngine &>(gameEngine).levelList()) {
            if (meta.levelIndex == levelIndex) {
                return meta.unlocked;
            }
        }
    }

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

bool MainWindow::isLevelCleared(int levelIndex) const
{
    if (levelIndex >= 0 && levelIndex < levels.size()) {
        for (const LevelMeta &meta : const_cast<GameEngine &>(gameEngine).levelList()) {
            if (meta.levelIndex == levelIndex) {
                return meta.cleared;
            }
        }
    }

    return completedStageIndexes.contains(levelIndex);
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
    if (levelIndex < 0) {
        levelIndex = 0;
    }

    if (!isLevelUnlocked(levelIndex)) {
        statusBar()->showMessage("This stage is locked by the path order.", 2500);
        return;
    }
    if (levels.isEmpty()) {
        statusBar()->showMessage("This level is not available yet.", 2500);
        return;
    }

    if (levelIndex < 0 || levelIndex >= levels.size()) {
        statusBar()->showMessage("This stage has no JSON yet. Opening the preview map.", 2500);
        levelIndex = 0;
    }

    newlyUnlockedStageIndexes.clear();
    gameEngine.levels = levels;
    bool started = gameEngine.startLevel(levelIndex);
    if (!started && levelIndex == 0) {
        gameEngine.m_save.Unlock(0);
        gameEngine.m_save.Save();
        started = gameEngine.startLevel(levelIndex);
    }

    if (!started) {
        statusBar()->showMessage("Backend refused to start this level.", 2500);
        return;
    }
    currentLevelIndex = levelIndex;
    syncFromEngineState();
    ui->stackedWidget->setCurrentWidget(ui->gamePage);
    syncBgmToCurrentPage();
    refreshGameUi();
    if (mapView) {
        mapView->setFocus(Qt::OtherFocusReason);
    }
}
