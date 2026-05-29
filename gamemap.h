#ifndef GAMEMAP_H
#define GAMEMAP_H
#include<QString>
#include<QVector>
#include"leveldata.h"

struct MoveResult{
    bool success;
    QString event;
    QString eventId;
    QVector<QPoint>movePath;
};

class GameMap
{
public:
    GameMap(const LevelData* ptr);
    bool canGoIn(int tarX,int tarY);
    bool canGoIn(QPoint target);
    bool moveAccessibility(int tarX,int tarY);
    bool moveAccessibility(QPoint target);
    //返回能否从此格子走向非来路的其他格子
    bool canReach(int tarX,int tarY);
    bool canReach(QPoint target);
    QString currentRoomState(int tarX,int tarY);
    QString currentRoomState(QPoint target);
    QVector<QPoint> findPath(int tarX,int tarY,bool* success=nullptr);
    QVector<QPoint> findPath(QPoint target,bool* success=nullptr);
    MoveResult moveTo(int tarX,int tarY);
    MoveResult moveTo(QPoint target);
    void Clear(int tarX,int tarY);
    void Clear(QPoint target);

private:
    const LevelData* levelData;
    int prevX,prevY;
    int playerX,playerY;
    QVector<QVector<int>>cleared;

};


#endif // GAMEMAP_H
