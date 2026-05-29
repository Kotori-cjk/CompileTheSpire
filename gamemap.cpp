#include "gamemap.h"

GameMap::GameMap(const LevelData* ptr){
    levelData=ptr;
    if(ptr==nullptr){
        return;
    }
    playerX=ptr->startpos.x();
    playerY=ptr->startpos.y();
    prevX=prevY=-1;
    cleared=QVector<QVector<int>>(ptr->mapHeight,QVector<int>(ptr->mapWidth,0));
}
bool GameMap::canGoIn(int tarX,int tarY){
    return levelData->mapGrid[tarX][tarY]!="#";
}
bool GameMap::canGoIn(QPoint target){
    return canGoIn(target.x(),target.y());
}
bool GameMap::moveAccessibility(int tarX,int tarY){
    if(abs(playerX-tarX)+abs(playerY-tarY)!=1){
        return false;
    }
    QString s=levelData->mapGrid[tarX][tarY];
    if(s=="#"){
        return false;
    }
    else if(s=="."){
        return true;
    }
    else if(s=="start"){
        return true;
    }
    else if(s=="boss"){
        return false;
    }
    QString op=s.mid(0,3);
    if(op=="mon"){
        QPoint pos=levelData->monsters[s].pos;
        return cleared[pos.x()][pos.y()];
    }
    if(op=="clu"){
        return true;
    }
    if(op=="che"){
        Chest c=levelData->chests[s];
        return cleared[c.pos.x()][c.pos.y()]||(!c.forcedPick);
    }
    return false;
}
bool GameMap::moveAccessibility(QPoint target){
    return moveAccessibility(target.x(),target.y());
}

QVector<QPoint> GameMap::findPath(int tarX,int tarY,bool* success){
    //wip
    Q_UNUSED(tarX);
    Q_UNUSED(tarY);
    if (success) {
        *success = false;
    }
    return {};
}
