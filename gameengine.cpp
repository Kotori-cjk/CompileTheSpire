#include "gameengine.h"

GameEngine::GameEngine(){
    m_bag=nullptr;
    m_map=nullptr;
    m_combat=nullptr;
    m_level=nullptr;
}
GameSnapshot GameEngine::getCurrentSnapshot(){
    if(m_level==nullptr||m_bag==nullptr||m_map==nullptr)return {};
    GameSnapshot ret;
    QPoint playerpos=m_map->playerPos();
    QPoint prevpos=m_map->prevPos();
    ret.playerX=playerpos.x(),ret.playerY=playerpos.y();
    ret.prevX=prevpos.x(),ret.prevY=prevpos.y();
    ret.levelData=m_level;
    ret.cleared=m_map->cleared;
    ret.bagBlocks=m_bag->bag();
    ret.leftBlocks=m_bag->leftBlocks;
    return ret;
}
void GameEngine::gameInit(QString path){
    QStringList errors;
    levels=LoadDirectory(path,&errors);
    m_save.Init(levels.size());
    m_save.Load(levels.size());
}
QVector<LevelMeta> GameEngine::levelList(){
    QVector<LevelMeta>ret;
    for(int i=0;i<levels.size();i++){
        const auto& cur=levels[i];
        LevelMeta now;
        now.levelIndex=i;
        now.unlocked=m_save.isUnlocked(i);
        now.levelType=cur.levelType;
        now.level=&cur;
        ret.append(now);
    }
    return ret;
}
bool GameEngine::startLevel(int levelIndex){
    if(levelIndex<0||levelIndex>=levels.size()){
        return false;
    }
    if(!m_save.isUnlocked(levelIndex)){
        return false;
    }
    if(m_bag!=nullptr)delete m_bag;
    if(m_map!=nullptr)delete m_map;
    m_bag=new Inventory(&levels[levelIndex]);
    m_map=new GameMap(&levels[levelIndex]);
    m_level=&levels[levelIndex];
    snapshotStack=QVector<GameSnapshot>();
    snapshotStack.append(getCurrentSnapshot());
    emit levelLoaded();
    return true;
}
bool GameEngine::moveTo(int tarX,int tarY){
    bool success=false;
    MoveResult res=m_map->moveTo(tarX,tarY,&success);
    if(!success){
        return false;
    }
    emit moveCompleted(res);
    if(res.event!="empty"){
        snapshotStack.append(getCurrentSnapshot());
        if(res.event=="clue"){
            QString clueId=res.eventId;
            for(const auto& monsterId:m_level->monsters.keys()){
                const auto& mcd=m_map->getMonsterClueDetail(monsterId);
                if(mcd.clueUnlockStates.contains(clueId)){
                    emit clueRevealed(clueId,monsterId,m_level->clues[clueId].val);
                    break;
                }
            }
        }
        if(res.event=="chest"){
            emit chestEntered(res.eventId);
        }
        else{
            QString& monsterId=res.eventId;
            Monster monster;
            monster=m_level->monsters[monsterId];
            auto mcd=m_map->getMonsterClueDetail(monsterId);
            QMap<QString,Clue>clues;
            for(const auto& clueId:mcd.clueIds){
                clues[clueId]=m_level->clues[clueId];
            }
            if(m_combat!=nullptr)delete m_combat;
            if(res.event=="monster"){
                m_combat=new Combat(monster,clues);
            }
            else m_combat=new Combat(m_level->boss,clues);
            emit combatStarted(res.eventId,m_combat);
        }
    }
    return true;
}
bool GameEngine::moveTo(QPoint target){
    return moveTo(target.x(),target.y());
}
bool GameEngine::moveDir(int dx,int dy){
    if(abs(dx)+abs(dy)!=1)return false;
    return moveTo(m_map->playerPos().x()+dx,m_map->playerPos().y()+dy);
}


// ---------

bool GameEngine::fillSpace(const QString& spaceId,const QString& blockId) {
    if(m_combat == nullptr || m_bag == nullptr){
        return false;
    }
    bool success = false;
    CodeBlock block = m_bag->bagGet(blockId, &success);
    if(!success){
        return false;
    }
    Space space = m_combat->id2Space(spaceId, &success);
    if(!success){
        return false;
    }
    return m_combat->submitBlock(space, block);
}

bool GameEngine::revealClue(const QString &clueId) {
    if(m_level == nullptr || m_map == nullptr){
        return false;
    }
    if(!m_level->clues.contains(clueId)){
        return false;
    }
    QPoint pos = m_level->clues[clueId].pos;
    m_map->Clear(pos);
    for(const auto& monsterId:m_level->monsters.keys()){
        const auto& mcd=m_map->getMonsterClueDetail(monsterId);
            if(mcd.clueUnlockStates.contains(clueId)){
                emit clueRevealed(clueId,monsterId,m_level->clues[clueId].val);
                break;
            }
         }
    return true;
}