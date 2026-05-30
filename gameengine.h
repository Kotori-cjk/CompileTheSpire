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
};

struct LevelMeta{
    int levelIndex;//the id in the vector
    LevelData* level;
    bool unlocked;
    QString levelType;
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

signals:
    void levelLoaded();
    void moveCompleted(MoveResult result);
    void combatStarted(QString monsterId,Combat* combat);
    void combatEnded(CombatResult result);
    void clueRevealed(QString clueId,QString monsterId,QString text);
    void chestEntered(QString chestId);
    void stateRestored();
    void exitLevel();//only emited when battle win(not included when player exit)

    GameEngine();
    void gameInit(QString path);
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
    bool startCombat(const QString& monsterId);
    //call it when a combat start(extrally for m_combat needs update)
    bool fillSpace(const QString& spaceId,const QString& blockId);
    //call it when player fill the space
    bool revealClue(const QString& clueId);
    //call it when player exit a clue(deal some afterthing)
    bool exitChest(const QString& chestId);
    //for develop:to go back to prev pos;technically I don't need the id,but I may check it
    //call it if player leave a **forced_pick** chest
    //if player skip an unforced,it seems that you don't need to call anything
    bool takeFromChest(const QString& chestId,const QString& blockId);
    //call it when player choose a block in chest
    bool exitCombat();
    //for develop:aka endCombat, similar func to exitChest
    //call it when player exit a combat
    bool submitCombat();
    //for develop:be care that if it's a boss battle(if win, update unlock .etc)
    //call it when player submit a combat
    QVector<CodeBlock> bagBlocks();
    //call it to get blocks in bag
    monsterClueDetail getMonsterDetail(const QString& monsterId);
    //call it to get monster detail, struct in gamemap.h, use it with templateBreakdown() thanks meow
    //btw, when in combat, you may combine two method to deal with the esapes(another is filling state in combat.h)
    bool undo();
    //call it when undo
    bool resetLevel();
    //call it when reset
    void exit();
    //call it when Exit(by any mean, include auto-calling when win or player exit)

};

#endif // GAMEENGINE_H
