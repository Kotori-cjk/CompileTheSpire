#ifndef GAMEMAP_H
#define GAMEMAP_H
#include<QString>
#include<QVector>
#include<QQueue>
#include<QSet>
#include"leveldata.h"

struct MoveResult{
    bool success;
    QString event;
    //"clue" "chest" "monster" "boss" "empty"
    QString eventId;
    //equal to mapString
    QVector<QPoint>movePath;
};

struct node{
    int x,y;
    QVector<QPoint>moves;
    node(int x,int y);
};

struct monsterClueDetail{
    QString codeTemplate;
    QMap<QString,bool>clueUnlockStates;
    QStringList clueIds;
    int revealedCount;
};

inline constexpr int dx[4] = {0, 0, 1, -1};
inline constexpr int dy[4] = {1, -1, 0, 0};

class GameMap
{
public:
    GameMap(const LevelData* ptr);
    QVector<QVector<int>>cleared;
    QMap<QString,QString>defeatedCodes;
    QSet<QPoint>blocked;

    QString currentId(int tarX,int tarY);
    QString currentId(QPoint target);
    bool canGoIn(int tarX,int tarY);
    bool canGoIn(QPoint target);
    bool moveAccessibility(int tarX,int tarY);
    bool moveAccessibility(QPoint target);
    //返回能否从此格子走向非来路的其他格子
    QString getEvent(int tarX,int tarY);
    QString getEvent(QPoint target);
    QVector<QPoint> findPath(int tarX,int tarY,bool* success=nullptr);
    QVector<QPoint> findPath(QPoint target,bool* success=nullptr);
    bool canReach(int tarX,int tarY);
    bool canReach(QPoint target);
    MoveResult moveTo(int tarX,int tarY,bool* success=nullptr);
    MoveResult moveTo(QPoint target,bool* success=nullptr);
    void Clear(int tarX,int tarY);
    void Clear(QPoint target);
    bool clueRevealed(QString clue)const;
    monsterClueDetail getMonsterClueDetail(QString monsterId)const;
    QPoint playerPos()const;
    QPoint prevPos()const;
    bool ifDefeated(const QString& monsterId)const;
    QString defeatedCode(const QString& monsterId)const;
    void setDefeatedCode(const QString& monsterId,const QString& code);
    QVector<Monster>monsterLeft()const;
    void setPlayerPos(int x,int y);
    void setPlayerPos(QPoint pos);
    void setPrevPos(int x,int y);
    void setPrevPos(QPoint pos);


private:
    const LevelData* levelData;
    int prevX,prevY;
    int playerX,playerY;


};


#endif // GAMEMAP_H
