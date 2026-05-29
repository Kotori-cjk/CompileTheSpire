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
QString GameMap::currentId(int tarX,int tarY){
    if(cleared[tarX][tarY])return "space";
    return levelData->mapGrid[tarX][tarY];
}
QString GameMap::currentId(QPoint target){
    return currentId(target.x(),target.y());
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
QString GameMap::getEvent(int tarX,int tarY){
    QString s=levelData->mapGrid[tarX][tarY];
    int stat=cleared[tarX][tarY];
    if(s=="."||s=="#"||s=="start")return "empty";
    QString op=s.mid(0,3);
    if(op=="clu"&&!stat){
        return "clue";
    }
    if(op=="che"&&!stat){
        return "chest";
    }
    if(op=="mon"&&!stat){
        return "monster";
    }
    return "boss";
}
QString GameMap::getEvent(QPoint target){
    return getEvent(target.x(),target.y());
}

node::node(int x,int y){
    this->x=x,this->y=y;
    moves=QVector<QPoint>(1,QPoint(x,y));
}

QVector<QPoint> GameMap::findPath(int tarX,int tarY,bool* success){
    if (!levelData || tarX < 0 || tarX >= levelData->mapHeight || tarY < 0 || tarY >= levelData->mapWidth) {
        if (success) {
            *success = false;
        }
        return {};
    }

    const auto canStepOn = [this](int x, int y) {
        if (x < 0 || x >= levelData->mapHeight || y < 0 || y >= levelData->mapWidth) {
            return false;
        }
        const QString tileId = levelData->mapGrid[x][y];
        if (tileId == "#" || tileId == "boss") {
            return false;
        }
        if (tileId == "." || tileId == "start" || tileId.mid(0, 3) == "clu") {
            return true;
        }
        if (tileId.mid(0, 3) == "mon") {
            const QPoint pos = levelData->monsters[tileId].pos;
            return cleared[pos.x()][pos.y()] != 0;
        }
        if (tileId.mid(0, 3) == "che") {
            const Chest chest = levelData->chests[tileId];
            return cleared[chest.pos.x()][chest.pos.y()] || !chest.forcedPick;
        }
        return false;
    };

    QVector<QVector<int>> vis(levelData->mapHeight, QVector<int>(levelData->mapWidth, 0));
    node st(playerX, playerY);
    vis[playerX][playerY] = 1;
    QQueue<node> q;
    q.push_back(st);
    while (!q.empty()) {
        const node u = q.front();
        q.pop_front();
        if (u.x == tarX && u.y == tarY) {
            if (success) {
                *success = true;
            }
            return u.moves;
        }
        for (int i = 0; i < 4; i++) {
            const int xx = u.x + dx[i];
            const int yy = u.y + dy[i];
            if (canStepOn(xx, yy) && !vis[xx][yy]) {
                vis[xx][yy] = 1;
                node nxt(xx, yy);
                nxt.moves = u.moves;
                nxt.moves.push_back(QPoint(xx, yy));
                q.push_back(nxt);
            }
        }
    }
    if (success) {
        *success = false;
    }
    return {};
}
QVector<QPoint> GameMap::findPath(QPoint target,bool* success){
    return findPath(target.x(),target.y(),success);
}
bool GameMap::canReach(int tarX,int tarY){
    bool* p=new bool(false);
    findPath(tarX,tarY,p);
    bool f=*p;
    delete p;
    return f;
}
bool GameMap::canReach(QPoint target){
    return canReach(target.x(),target.y());
}
MoveResult GameMap::moveTo(int tarX,int tarY,bool* success){
    MoveResult res;
    bool* p=new bool(false);
    auto path=findPath(tarX,tarY,p);
    res.success=*p;
    res.movePath=path;
    if(success!=nullptr)*success=*p;
    if(!*p){
        res.event="empty";
        res.eventId="";
        delete p;
        return res;
    }
    res.event=getEvent(tarX,tarY);
    res.eventId=levelData->mapGrid[tarX][tarY];
    playerX=tarX,playerY=tarY;
    int len=res.movePath.size();
    if(len>1){
        prevX=res.movePath[len-2].x();
        prevY=res.movePath[len-2].y();
    }
    delete p;
    return res;
}
MoveResult GameMap::moveTo(QPoint target,bool* success){
    return moveTo(target.x(),target.y(),success);
}
void GameMap::Clear(int tarX,int tarY){
    cleared[tarX][tarY]=1;
}
void GameMap::Clear(QPoint target){
    Clear(target.x(),target.y());
}

bool GameMap::clueRevealed(QString clue)const{
    QPoint pos=levelData->clues[clue].pos;
    return cleared[pos.x()][pos.y()];
}

monsterClueDetail GameMap::getMonsterClueDetail(QString monsterId)const{
    monsterClueDetail ret;
    ret.codeTemplate=levelData->monsters[monsterId].codeTemplate;
    Synthesis synthesis=templateBreakdown(ret.codeTemplate);
    for(int i=0;i<synthesis.cell.size();i++){
        if(synthesis.cell[i].type=="clue"){
            ret.clueUnlockStates[synthesis.cell[i].id]=clueRevealed(synthesis.cell[i].id);
        }
    }
    return ret;
}

QPoint GameMap::playerPos()const{
    return QPoint(playerX,playerY);
}