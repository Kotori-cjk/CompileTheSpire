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
    if(cleared[tarX][tarY])return ".";
    if(blocked.contains(QPoint(tarX,tarY)))return "#";
    return levelData->mapGrid[tarX][tarY];
}
QString GameMap::currentId(QPoint target){
    return currentId(target.x(),target.y());
}
bool GameMap::canGoIn(int tarX,int tarY){
    if(blocked.contains(QPoint(tarX,tarY)))return false;
    return levelData->mapGrid[tarX][tarY]!="#";
}
bool GameMap::canGoIn(QPoint target){
    return canGoIn(target.x(),target.y());
}
bool GameMap::moveAccessibility(int tarX,int tarY){
    if(blocked.contains(QPoint(tarX,tarY)))return false;
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
    if(blocked.contains(QPoint(tarX,tarY)))return "empty";
    QString s=levelData->mapGrid[tarX][tarY];
    int stat=cleared[tarX][tarY];
    if(stat||s=="."||s=="#"||s=="start")return "empty";
    QString op=s.mid(0,3);
    if(op=="clu"){
        return "clue";
    }
    if(op=="che"){
        return "chest";
    }
    if(op=="mon"){
        return "monster";
    }
    if(s=="boss")return "boss";
    return "empty";
}
QString GameMap::getEvent(QPoint target){
    return getEvent(target.x(),target.y());
}

node::node(int x,int y){
    this->x=x,this->y=y;
    moves=QVector<QPoint>(1,QPoint(x,y));
}
QVector<QPoint> GameMap::findPath(int tarX,int tarY,bool* success){
    if (!levelData||tarX<0||tarX>=levelData->mapHeight||tarY<0||tarY>=levelData->mapWidth){
        if(success!=nullptr)*success=false;
        return QVector<QPoint>();
    }
    QVector<QVector<int>> vis(levelData->mapHeight,QVector<int>(levelData->mapWidth,0));
    node st(playerX,playerY);
    vis[playerX][playerY]=1;
    QQueue<node>q;
    q.push_back(st);
    while(!q.empty()){
        node u=q.front();
        q.pop_front();
        if(u.x==tarX&&u.y==tarY){
            if(success!=nullptr)*success=true;
            return u.moves;
        }
        for(int i=0;i<4;i++){
            int xx=u.x+dx[i],yy=u.y+dy[i];
            if(xx>=0&&xx<levelData->mapHeight&&yy>=0&&yy<levelData->mapWidth&&(u.x==playerX&&u.y==playerY||moveAccessibility(u.x,u.y))&&canGoIn(xx,yy)&&!vis[xx][yy]){
                vis[xx][yy]=1;
                node nxt(xx,yy);
                nxt.moves=u.moves;
                nxt.moves.push_back(QPoint(xx,yy));
                q.push_back(nxt);
                if(xx==tarX&&yy==tarY){
                    if(success!=nullptr)*success=true;
                    return nxt.moves;
                }
            }
        }
    }
    if(success!=nullptr)*success=false;
    return QVector<QPoint>();
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
            ret.clueIds.append(synthesis.cell[i].id);
            ret.revealedCount+=clueRevealed(synthesis.cell[i].id);
        }
    }
    return ret;
}
QPoint GameMap::playerPos()const{
    return QPoint(playerX,playerY);
}
QPoint GameMap::prevPos()const{
    return QPoint(prevX,prevY);
}
bool GameMap::ifDefeated(const QString& monsterId)const{
    QPoint pos=levelData->monsters[monsterId].pos;
    return cleared[pos.x()][pos.y()];
}
QString GameMap::defeatedCode(const QString& monsterId)const{
    return defeatedCodes.value(monsterId);
}
void GameMap::setDefeatedCode(const QString& monsterId,const QString& code){
    defeatedCodes[monsterId]=code;
}
QVector<Monster> GameMap::monsterLeft()const{
    QVector<Monster>ret;
    for(const auto& monsterId:levelData->monsters.keys()){
        const Monster& monster=levelData->monsters[monsterId];
        QPoint pos=monster.pos;
        if(!cleared[pos.x()][pos.y()]){
            ret.append(monster);
        }
    }
    return ret;
}
void GameMap::setPlayerPos(int x,int y){
    playerX=x,playerY=y;
}
void GameMap::setPlayerPos(QPoint pos){
    setPlayerPos(pos.x(),pos.y());
}
void GameMap::setPrevPos(int x,int y){
    prevX=x,prevY=y;
}
void GameMap::setPrevPos(QPoint pos){
    setPrevPos(pos.x(),pos.y());
}
