#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "gameengine.h"
#include "leveldata.h"

#include <QAudioOutput>
#include <QLabel>
#include <QMediaPlayer>
#include <QMainWindow>
#include <QMap>
#include <QPushButton>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MapView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupMovementShortcuts();
    bool handleGameMoveKey(int key);
    void applyDefaultSettings();
    void applyVisualStyle();
    void buildRuntimeGameUi();
    void positionMainMenuButtons();
    void updateMainMenuBackground();
    void playBgm(const QString &resourcePath);
    void syncBgmToCurrentPage();
    void updateBgmVolume();
    void loadLevels();
    void refreshLevelSelectUi();
    void startLevel(int levelIndex);
    void setLevelSelectMode(bool exMode);
    void changeLevelPage(int delta);
    void selectStage(int levelIndex);
    bool isLevelUnlocked(int levelIndex) const;
    void resetLevel();
    void undo();

    void refreshGameUi();
    void syncFromEngineState();
    void refreshMapGrid();
    void refreshSidePanel();
    void showBagDialog();
    void showManualDialog();
    void showVictorySettlement();

    void movePlayer(int rowDelta, int columnDelta);
    void movePlayerTo(int targetRow, int targetColumn);
    void startMovePlayback(const QVector<QPoint> &backendPath, int targetRow, int targetColumn);
    void advanceMovePlayback();
    bool moveThroughEngine(int targetRow, int targetColumn);
    void clearDisplayedMovePath();
    bool canEnter(int row, int column) const;
    QString tileAt(int row, int column) const;
    QString describeTile(const QString &tileId) const;
    void handleChest(const QString &chestId);
    void handleMonster(const QString &monsterId);

    QString renderMonsterCode(const Monster &monster) const;
    QString renderMonsterCodeHtml(const Monster &monster) const;
    QString codeForBlock(const QString &blockId) const;
    QString typeForBlock(const QString &blockId) const;
    CodeBlock codeBlockForId(const QString &blockId) const;
    QString codeBlockIconPath(const QString &blockId) const;
    bool chestHasAvailableBlocks(const QString &chestId) const;
    Monster monsterByTile(const QString &tileId) const;

    Ui::MainWindow *ui;
    QVector<LevelData> levels;
    GameEngine gameEngine;
    int currentLevelIndex = -1;
    int playerRow = 0;
    int playerColumn = 0;
    QStringList bagBlocks;
    QMap<QString, CodeBlock> knownCodeBlocks;
    QSet<QString> collectedClues;
    QSet<QString> openedChests;
    QSet<QString> defeatedMonsters;
    QSet<QString> seenMonsters;
    QSet<int> completedStageIndexes;
    int currentLevelSelectPage = 0;
    QVector<QPoint> activeMovePath;
    int activeMovePathIndex = 0;
    int pendingMoveTargetRow = -1;
    int pendingMoveTargetColumn = -1;
    bool movePlaybackActive = false;
    bool suppressNextMovePath = false;
    bool showingExLevels = false;
    int selectedStageIndex = 0;
    QString currentBgmResource;

    MapView *mapView = nullptr;
    QLabel *tileInfoLabel = nullptr;
    QPushButton *resetRunButton = nullptr;
    QPushButton *handbookButton = nullptr;
    QWidget *mainMenuButtonBar = nullptr;
    QMediaPlayer *bgmPlayer = nullptr;
    QAudioOutput *bgmAudioOutput = nullptr;
};

#endif // MAINWINDOW_H
