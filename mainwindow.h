#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "leveldata.h"

#include <QLabel>
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

struct UiGameSnapshot
{
    int row = 0;
    int column = 0;
    QStringList bagBlocks;
    QMap<QString, CodeBlock> knownCodeBlocks;
    QMap<QString, QSet<QString>> remainingChestBlocks;
    QSet<QString> collectedClues;
    QSet<QString> openedChests;
    QSet<QString> defeatedMonsters;
    QSet<QString> seenMonsters;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyDefaultSettings();
    void applyVisualStyle();
    void buildRuntimeGameUi();
    void positionMainMenuButtons();
    void updateMainMenuBackground();
    void loadLevels();
    void refreshLevelSelectUi();
    void startLevel(int levelIndex);
    void setLevelSelectMode(bool exMode);
    void changeLevelPage(int delta);
    void selectStage(int levelIndex);
    bool isLevelUnlocked(int levelIndex) const;
    void resetLevel();
    void pushUndoState();
    void undo();

    void refreshGameUi();
    void refreshMapGrid();
    void refreshSidePanel();
    void refreshBagPage();
    void refreshManualPage();
    void showBagDialog();
    void showManualDialog();
    void showVictorySettlement();

    void movePlayer(int rowDelta, int columnDelta);
    void movePlayerTo(int targetRow, int targetColumn);
    void advanceAutoMove();
    void cancelAutoMove(bool returnToPreviousPoint);
    bool stepPlayerTo(int row, int column, bool saveUndo);
    bool canEnter(int row, int column) const;
    bool canUseAsPathNode(int row, int column, const QPoint &target) const;
    QString tileAt(int row, int column) const;
    QString describeTile(const QString &tileId) const;
    bool hasTileEvent(const QString &tileId) const;
    bool isSkippablePathChest(const QString &tileId) const;
    void triggerTileEvent(bool fromAutoMove);
    void returnToPreviousTile();
    void interactWithCurrentTile();
    void handleChest(const QString &chestId);
    void handleMonster(const QString &monsterId);
    void submitFill();

    QString renderMonsterCode(const Monster &monster) const;
    QString renderMonsterCodeHtml(const Monster &monster) const;
    QString codeForBlock(const QString &blockId) const;
    QString typeForBlock(const QString &blockId) const;
    CodeBlock codeBlockForId(const QString &blockId) const;
    QString codeBlockIconPath(const QString &blockId) const;
    CodeBlock synthesizeMonsterBlock(const Monster &monster, const QStringList &blocks) const;
    bool chestHasAvailableBlocks(const QString &chestId) const;
    bool blockMatchesSpace(const QString &blockId, const Space &space) const;
    QStringList splitAnswerBlocks(const QString &text) const;
    Monster monsterByTile(const QString &tileId) const;

    Ui::MainWindow *ui;
    QVector<LevelData> levels;
    int currentLevelIndex = -1;
    int playerRow = 0;
    int playerColumn = 0;
    QStringList bagBlocks;
    QMap<QString, CodeBlock> knownCodeBlocks;
    QMap<QString, QSet<QString>> remainingChestBlocks;
    QSet<QString> collectedClues;
    QSet<QString> openedChests;
    QSet<QString> defeatedMonsters;
    QSet<QString> seenMonsters;
    QSet<int> completedStageIndexes;
    QVector<UiGameSnapshot> history;
    int currentLevelSelectPage = 0;
    int currentBagPage = 0;
    int previousPlayerRow = 0;
    int previousPlayerColumn = 0;
    QVector<QPoint> activeMovePath;
    int activeMovePathIndex = 0;
    bool autoMoving = false;
    bool showingExLevels = false;
    int selectedStageIndex = 0;

    //---manual part---
    QString manualSelectedMonsterId;

    MapView *mapView = nullptr;
    QLabel *tileInfoLabel = nullptr;
    QPushButton *resetRunButton = nullptr;
    QPushButton *handbookButton = nullptr;
    QWidget *mainMenuButtonBar = nullptr;
};

#endif // MAINWINDOW_H
