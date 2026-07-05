#ifndef GAMEENGINE_H
#define GAMEENGINE_H

#include"gamemap.h"
#include"inventory.h"
#include"combat.h"
#include"leveldata.h"
#include"savemanager.h"
#include<QVector>
#include<QString>
#include<QStringList>
#include<QPoint>
#include<QSet>

struct GameSnapshot{
    int playerX,playerY;
    int prevX,prevY;
    const LevelData* levelData;
    QVector<QVector<int>>cleared;
    QVector<CodeBlock>bagBlocks;
    QMap<QString,QSet<QString>>leftBlocks;
    QMap<QString,QString>defeatedCodes;
    QSet<QPoint>blocked;
};

struct LevelMeta{
    int levelIndex;//the id in the vector
    const LevelData* level;
    bool unlocked;
    bool cleared;
    QString levelType;
    QString levelName;//filename without .json
};

class GameEngine:public QObject
{
    Q_OBJECT
public:
    QVector<LevelData>levels;
    SaveManager m_save;
    Inventory* m_bag;
    GameMap* m_map;
    Combat* m_combat;
    LevelData* m_level;
    //warn:deal with memory-related operation carefully
    QVector<GameSnapshot>snapshotStack;
    bool m_locked=false;

signals:
    void levelLoaded();
    void moveCompleted(MoveResult result);
    void combatStarted(QString monsterId,Combat* combat);
    void combatEnded(CombatResult result);
    void clueRevealed(QString clueId,QString monsterId,QString text);
    void chestEntered(QString chestId,bool m_locked);
    void forcedMove(QPoint newpos);
    //when forced move(by exit), trigger this and change physical location
    void exitLevel();
    void levelUnlocked(int levelIndex);
    void moveBlocked(QString reason);

public:
    GameSnapshot getCurrentSnapshot();
    void restoreFromSnapshot(GameSnapshot snapshot);
    GameEngine();
    void gameInit(QString path,const QString& listFile="");
    //for develop:look to savemanager to recall the things you should do(Init() in it)
    //call it when game start
    QVector<LevelMeta> levelList();
    //call it to get levels information
    bool startLevel(int levelIndex);
    //call it when a level is selected and started
    bool moveDir(int dx,int dy);
    //call it when move towards a direction (with keyboard)
    bool moveTo(int tarX,int tarY);
    bool moveTo(QPoint target);
    //for develop:if trigger an event, then save the snapshot
    //call these when move by click (with mouse)
    bool fillSpace(const QString& spaceId,const QString& blockId);
    //call it when player fill the space
    bool unfillSpace(const QString& spaceId);
    //call it when player remove a block from space
    bool revealClue(const QString& clueId);
    //call it when player trigger a clue
    bool exitChest(const QString& chestId);
    //for develop:to go back to prev pos;technically I don't need the id,but I may check it
    //call it if player leave a **forced_pick** chest
    //if player skip an unforced,it seems that you don't need to call anything
    bool takeFromChest(const QString& chestId,const QString& blockId);
    //call it when player choose a block in chest
    bool takeBundleFromChest(const QString& chestId);
    //call it when player confirm bundle pick
    bool exitCombat();
    //for develop:aka endCombat, similar func to exitChest
    //call it when player exit a combat
    CombatResult submitCombat();
    //for develop:be care that if it's a boss battle(if win, update unlock .etc)
    //call it when player submit a combat
    QVector<CodeBlock> bagBlocks();
    //call it to get blocks in bag
    bool discardBlock(const QString& blockId);
    //call it when player discard a block, push snapshot
    monsterClueDetail getMonsterDetail(const QString& monsterId);
    //call it to get monster detail, struct in gamemap.h, use it with templateBreakdown() thanks meow
    //btw, when in combat, you may combine two method to deal with the esapes(another is filling state in combat.h)
    bool undo();
    //call it when undo
    bool resetLevel();
    //call it when reset
    void exit();
    //call it when Exit(only player exit)
    //no need to call when auto-exit at winning

};

#endif // GAMEENGINE_H
