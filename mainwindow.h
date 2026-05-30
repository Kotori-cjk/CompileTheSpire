#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "leveldata.h"

#include <QLabel>
#include <QMainWindow>
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

struct GameSnapshot
{
    int row = 0;
    int column = 0;
    QStringList bagBlocks;
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

    void movePlayer(int rowDelta, int columnDelta);
    void movePlayerTo(int targetRow, int targetColumn);
    bool canEnter(int row, int column) const;
    QString tileAt(int row, int column) const;
    QString describeTile(const QString &tileId) const;
    void interactWithCurrentTile();
    void handleChest(const QString &chestId);
    void handleMonster(const QString &monsterId);
    void submitFill();

    QString renderMonsterCode(const Monster &monster) const;
    bool blockMatchesSpace(const QString &blockId, const Space &space) const;
    QStringList splitAnswerBlocks(const QString &text) const;
    Monster monsterByTile(const QString &tileId) const;

    Ui::MainWindow *ui;
    QVector<LevelData> levels;
    int currentLevelIndex = -1;
    int playerRow = 0;
    int playerColumn = 0;
    QStringList bagBlocks;
    QSet<QString> openedChests;
    QSet<QString> defeatedMonsters;
    QSet<QString> seenMonsters;
    QSet<int> completedStageIndexes;
    QVector<GameSnapshot> history;
    int currentLevelSelectPage = 0;
    bool showingExLevels = false;
    int selectedStageIndex = 0;

    MapView *mapView = nullptr;
    QLabel *tileInfoLabel = nullptr;
    QPushButton *resetRunButton = nullptr;
    QPushButton *handbookButton = nullptr;
    QWidget *mainMenuButtonBar = nullptr;
};

#endif // MAINWINDOW_H
