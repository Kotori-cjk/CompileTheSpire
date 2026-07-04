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

void MainWindow::resetLevel()
{
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    gameEngine.levels = levels;
    gameEngine.startLevel(currentLevelIndex);
    syncFromEngineState();
    activeMovePath.clear();
    activeMovePathIndex = 0;
    movePlaybackActive = false;
    suppressNextMovePath = false;
    pendingMoveTargetRow = -1;
    pendingMoveTargetColumn = -1;
    ui->combatLogLabel->setText(beginnerTipsEnabled()
                                    ? "Use WASD/arrow keys or click a reachable tile."
                                    : "Ready.");
    playSfx("assets/audio/sfx_move.wav");
    refreshGameUi();
}

void MainWindow::undo()
{
    clearDisplayedMovePath();
    if (!gameEngine.m_map || !gameEngine.undo()) {
        ui->combatLogLabel->setText("Nothing to undo yet.");
        playSfx("assets/audio/sfx_error.wav");
        refreshGameUi();
        return;
    }

    syncFromEngineState();
    ui->combatLogLabel->setText("Undo restored the previous state.");
    playSfx("assets/audio/sfx_move.wav");
    refreshGameUi();
}

void MainWindow::refreshGameUi()
{
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    if (!movePlaybackActive) {
        syncFromEngineState();
    }
    const LevelData &level = levels.at(currentLevelIndex);
    ui->hpLabel->setText(QString("Level: %1").arg(currentLevelIndex + 1));
    ui->goldLabel->setText(QString("Bag: %1/%2").arg(bagBlocks.size()).arg(level.bagSize));
    ui->floorLabel->setText(QString("Pos: %1,%2").arg(playerColumn).arg(playerRow));
    ui->nextChallengeButton->setEnabled(gameEngine.snapshotStack.size() > 1);
    if (undoRunButton) {
        undoRunButton->setEnabled(gameEngine.snapshotStack.size() > 1);
    }
    refreshMapGrid();
    refreshSidePanel();
}

void MainWindow::syncFromEngineState()
{
    if (gameEngine.m_level) {
        currentLevelIndex = gameEngine.m_level->levelIndex;
    }

    if (!gameEngine.m_map || !gameEngine.m_bag || currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    const QPoint playerPos = gameEngine.m_map->playerPos();
    playerRow = playerPos.x();
    playerColumn = playerPos.y();

    bagBlocks.clear();
    knownCodeBlocks.clear();
    for (const CodeBlock &block : gameEngine.m_bag->bag()) {
        bagBlocks.append(block.blockId);
        knownCodeBlocks.insert(block.blockId, block);
    }
    openedChests.clear();
    collectedClues.clear();
    defeatedMonsters.clear();
    const LevelData &level = levels.at(currentLevelIndex);
    for (auto it = level.chests.cbegin(); it != level.chests.cend(); ++it) {
        const QPoint pos = it.value().pos;
        if (pos.x() >= 0 && pos.x() < gameEngine.m_map->cleared.size()
            && pos.y() >= 0 && pos.y() < gameEngine.m_map->cleared.at(pos.x()).size()
            && gameEngine.m_map->cleared.at(pos.x()).at(pos.y())) {
            openedChests.insert(it.key());
        }
    }
    for (auto it = level.clues.cbegin(); it != level.clues.cend(); ++it) {
        if (gameEngine.m_map->clueRevealed(it.key())) {
            collectedClues.insert(it.key());
        }
    }
    for (auto it = level.monsters.cbegin(); it != level.monsters.cend(); ++it) {
        const QPoint pos = it.value().pos;
        if (pos.x() >= 0 && pos.x() < gameEngine.m_map->cleared.size()
            && pos.y() >= 0 && pos.y() < gameEngine.m_map->cleared.at(pos.x()).size()
            && gameEngine.m_map->cleared.at(pos.x()).at(pos.y())) {
            defeatedMonsters.insert(it.key());
        }
    }
    if (level.boss.pos.x() >= 0 && level.boss.pos.x() < gameEngine.m_map->cleared.size()
        && level.boss.pos.y() >= 0 && level.boss.pos.y() < gameEngine.m_map->cleared.at(level.boss.pos.x()).size()
        && gameEngine.m_map->cleared.at(level.boss.pos.x()).at(level.boss.pos.y())) {
        defeatedMonsters.insert("boss");
    }
}

void MainWindow::refreshMapGrid()
{
    if (!mapView || currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    mapView->setLevel(&level);
    mapView->setPlayerPosition(QPoint(playerColumn, playerRow));
    mapView->setClearedObjects(openedChests, defeatedMonsters);
    mapView->setCollectedClues(collectedClues);
    mapView->setMovePath(activeMovePath, activeMovePathIndex);
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
            QString formatted = formatCodeBlockForDisplay(block.code);
            formatted.replace('\n', "\n    ");
            lines << QString("  %1 = %2").arg(block.blockId, formatted);
        }
        ui->challengeTextEdit->setPlainText(lines.join('\n'));
    } else if (tileId.startsWith("clue")) {
        const Clue clue = levels.at(currentLevelIndex).clues.value(tileId);
        ui->challengeTextEdit->setPlainText(clue.val.isEmpty() ? "No clue text." : clue.val);
    } else {
        ui->challengeTextEdit->setPlainText(beginnerTipsEnabled()
                                                ? "Move onto a chest, clue, monster, or boss tile and interact."
                                                : QString());
    }
}

void MainWindow::movePlayer(int rowDelta, int columnDelta)
{
    if (movePlaybackActive || (mapView && mapView->isPlayerAnimating())) {
        return;
    }
    clearDisplayedMovePath();
    if (!gameEngine.m_map) {
        ui->combatLogLabel->setText("No active map.");
        playSfx("assets/audio/sfx_error.wav");
        return;
    }

    const QPoint backendPos = gameEngine.m_map->playerPos();
    if (!moveThroughEngine(backendPos.x() + rowDelta, backendPos.y() + columnDelta)) {
        ui->combatLogLabel->setText("Blocked.");
        playSfx("assets/audio/sfx_error.wav");
    }
}

void MainWindow::movePlayerTo(int targetRow, int targetColumn)
{
    if (movePlaybackActive || (mapView && mapView->isPlayerAnimating())) {
        return;
    }
    if (!gameEngine.m_map || currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        ui->combatLogLabel->setText("No path to that tile.");
        playSfx("assets/audio/sfx_error.wav");
        return;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    if (targetRow < 0 || targetRow >= level.mapGrid.size()
        || targetColumn < 0 || targetColumn >= level.mapGrid.at(targetRow).size()
        || !gameEngine.m_map->canGoIn(targetRow, targetColumn)) {
        ui->combatLogLabel->setText("No path to that tile.");
        playSfx("assets/audio/sfx_error.wav");
        return;
    }

    bool success = false;
    const QVector<QPoint> backendPath = gameEngine.m_map->findPath(targetRow, targetColumn, &success);
    if (!success || backendPath.size() < 2) {
        const QString tileId = level.mapGrid.at(targetRow).at(targetColumn);
        const QPoint playerPos = gameEngine.m_map->playerPos();
        if (targetRow == playerPos.x() && targetColumn == playerPos.y()
            && tileId.startsWith("chest") && !openedChests.contains(tileId)) {
            handleChest(tileId, false);
            return;
        }
        if (tileId.startsWith("chest") && !openedChests.contains(tileId)) {
            ui->combatLogLabel->setText("Chest is out of reach.");
            handleChest(tileId, true);
            return;
        }
        ui->combatLogLabel->setText("No path to that tile.");
        playSfx("assets/audio/sfx_error.wav");
        return;
    }

    if (instantMoveMode || movePlaybackIntervalMs() <= 0) {
        activeMovePath.clear();
        activeMovePathIndex = 0;
        pendingMoveTargetRow = -1;
        pendingMoveTargetColumn = -1;
        suppressNextMovePath = true;
        if (!moveThroughEngine(targetRow, targetColumn)) {
            suppressNextMovePath = false;
            syncFromEngineState();
            refreshGameUi();
            ui->combatLogLabel->setText("No path to that tile.");
            playSfx("assets/audio/sfx_error.wav");
        }
        return;
    }

    startMovePlayback(backendPath, targetRow, targetColumn);
}

void MainWindow::startMovePlayback(const QVector<QPoint> &backendPath, int targetRow, int targetColumn)
{
    activeMovePath.clear();
    for (const QPoint &step : backendPath) {
        activeMovePath.append(QPoint(step.y(), step.x()));
    }
    activeMovePathIndex = 1;
    pendingMoveTargetRow = targetRow;
    pendingMoveTargetColumn = targetColumn;
    movePlaybackActive = true;

    const QPoint start = backendPath.first();
    playerRow = start.x();
    playerColumn = start.y();
    ui->combatLogLabel->setText(QString("Moving to %1,%2.").arg(targetColumn).arg(targetRow));
    refreshGameUi();
    QTimer::singleShot(movePlaybackIntervalMs(), this, &MainWindow::advanceMovePlayback);
}

void MainWindow::advanceMovePlayback()
{
    if (!movePlaybackActive) {
        return;
    }

    if (activeMovePathIndex >= activeMovePath.size()) {
        movePlaybackActive = false;
        activeMovePath.clear();
        activeMovePathIndex = 0;
        const int targetRow = pendingMoveTargetRow;
        const int targetColumn = pendingMoveTargetColumn;
        pendingMoveTargetRow = -1;
        pendingMoveTargetColumn = -1;
        suppressNextMovePath = true;
        if (!moveThroughEngine(targetRow, targetColumn)) {
            suppressNextMovePath = false;
            syncFromEngineState();
            refreshGameUi();
            ui->combatLogLabel->setText("No path to that tile.");
            playSfx("assets/audio/sfx_error.wav");
        }
        return;
    }

    const QPoint next = activeMovePath.at(activeMovePathIndex);
    playerRow = next.y();
    playerColumn = next.x();
    ++activeMovePathIndex;
    playSfx("assets/audio/sfx_move.wav");
    refreshGameUi();
    QTimer::singleShot(movePlaybackIntervalMs(), this, &MainWindow::advanceMovePlayback);
}

int MainWindow::movePlaybackIntervalMs() const
{
    if (!ui || !ui->animationSpeedComboBox) {
        return 115;
    }

    switch (ui->animationSpeedComboBox->currentIndex()) {
    case 1:
        return 55;
    case 2:
        return 0;
    default:
        return 115;
    }
}

bool MainWindow::moveThroughEngine(int targetRow, int targetColumn)
{
    if (!gameEngine.m_map || currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return false;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    if (targetRow < 0 || targetRow >= level.mapGrid.size()
        || targetColumn < 0 || targetColumn >= level.mapGrid.at(targetRow).size()) {
        return false;
    }

    if (!gameEngine.m_map->canGoIn(targetRow, targetColumn)) {
        return false;
    }

    return gameEngine.moveTo(targetRow, targetColumn);
}

void MainWindow::clearDisplayedMovePath()
{
    if (!movePlaybackActive && activeMovePath.isEmpty()) {
        return;
    }

    movePlaybackActive = false;
    pendingMoveTargetRow = -1;
    pendingMoveTargetColumn = -1;
    activeMovePath.clear();
    activeMovePathIndex = 0;
    refreshGameUi();
}

bool MainWindow::canEnter(int row, int column) const
{
    if (!gameEngine.m_map || currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return false;
    }
    const LevelData &level = levels.at(currentLevelIndex);
    return row >= 0
           && row < level.mapGrid.size()
           && column >= 0
           && column < level.mapGrid.at(row).size()
           && gameEngine.m_map->canGoIn(row, column);
}

QString MainWindow::tileAt(int row, int column) const
{
    if (!canEnter(row, column)) {
        return "#";
    }
    return gameEngine.m_map ? gameEngine.m_map->currentId(row, column) : levels.at(currentLevelIndex).mapGrid.at(row).at(column);
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
