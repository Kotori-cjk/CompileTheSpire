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
void GameEngine::restoreFromSnapshot(GameSnapshot snapshot){
    if(snapshot.levelData!=m_level){
        throw "encounter bug at snapshot loading";
        return;
    }
    m_map->setPlayerPos(snapshot.playerX,snapshot.playerY);
    m_map->setPrevPos(snapshot.prevX,snapshot.prevY);
    m_map->cleared=snapshot.cleared;
    m_bag->leftBlocks=snapshot.leftBlocks;
    m_bag->bagBlocks=snapshot.bagBlocks;
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
    m_level->levelIndex=levelIndex;
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
        else if(res.event=="chest"){
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
bool GameEngine::fillSpace(const QString& spaceId,const QString& blockId){
    bool success=false;
    Space spc=m_combat->id2Space(spaceId,&success);
    if(!success)return false;
    if(!m_bag->bagContains(blockId))return false;
    CodeBlock tblock;
    for(const auto& block:m_bag->bag()){
        if(block.blockId==blockId){
            tblock=block;
            break;
        }
    }
    return m_combat->submitBlock(spc,tblock);
}
bool GameEngine::revealClue(const QString& clueId){
    if(m_map->clueRevealed(clueId))return false;
    if(!m_level->clues.contains(clueId))return false;
    QPoint cpos=m_level->clues[clueId].pos;
    m_map->Clear(cpos);
    snapshotStack.append(getCurrentSnapshot());
    Monster tmonster;
    for(const auto& monster:m_level->monsters.values()){
        if(monster.referencedClues.contains(clueId)){
            tmonster=monster;
            break;
        }
    }
    emit clueRevealed(clueId,tmonster.monsterId,m_level->clues[clueId].val);
    return true;
}
bool GameEngine::exitChest(const QString& chestId){
    snapshotStack.removeLast();
    m_map->setPlayerPos(m_map->prevPos());
    emit forcedMove(m_map->prevPos());
    return true;
}
bool GameEngine::takeFromChest(const QString& chestId,const QString& blockId){
    if(!m_level->chests.contains(chestId))return false;
    bool success=false;
    QString msg="";
    success=m_bag->addBlockFromChest(blockId,chestId,&msg);
    if(!success)return false;
    snapshotStack.append(getCurrentSnapshot());
    if(!m_level->chests[chestId].repeat||m_bag->chestIsEmpty(chestId)){
        m_map->Clear(m_level->chests[chestId].pos);
    }
    return true;
}
bool GameEngine::exitCombat(){
    if(m_combat!=nullptr){
        delete m_combat;
        m_combat=nullptr;
        snapshotStack.removeLast();
    }
    m_map->setPlayerPos(m_map->prevPos());
    emit forcedMove(m_map->prevPos());
    return true;
}
CombatResult GameEngine::submitCombat(){
    auto result=m_combat->submitCombat();
    if(result.resultType=="success"){
        if(m_combat->isBoss()){
            //win
            emit combatEnded(result);
            if(m_combat!=nullptr)delete m_combat;
            m_combat=nullptr;
            if(m_map!=nullptr)delete m_map;
            m_map=nullptr;
            if(m_bag!=nullptr)delete m_bag;
            m_bag=nullptr;
            snapshotStack.clear();
            if(m_save.Unlock(m_level->levelIndex+1)){
                m_save.Save();
                emit levelUnlocked(m_level->levelIndex+1);
            }
            emit exitLevel();
            return result;
        }
        else{
            for(const auto& blockId:result.usedBlocks){
                m_bag->bagRemove(blockId);
            }
            m_bag->bagAdd(result.synthesizedBlock);
            m_map->Clear(m_level->monsters[m_combat->monsterId()].pos);
            snapshotStack.append(getCurrentSnapshot());
            emit combatEnded(result);
            if(m_combat!=nullptr)delete m_combat;
            m_combat=nullptr;
            return result;
        }
    }
    return result;
}
QVector<CodeBlock> GameEngine::bagBlocks(){
    return m_bag->bag();
}
monsterClueDetail GameEngine::getMonsterDetail(const QString& monsterId){
    return m_map->getMonsterClueDetail(monsterId);
}
bool GameEngine::undo(){
    if(snapshotStack.length()<=1){
        return false;
    }
    restoreFromSnapshot(snapshotStack[snapshotStack.length()-2]);
    snapshotStack.removeLast();
    emit forcedMove(m_map->playerPos());
    return true;
}
bool GameEngine::resetLevel(){
    restoreFromSnapshot(snapshotStack[0]);
    snapshotStack=QVector<GameSnapshot>();
    snapshotStack.append(getCurrentSnapshot());
    emit forcedMove(m_map->playerPos());
    return true;
}
void GameEngine::exit(){
    if(m_combat!=nullptr)delete m_combat;
    m_combat=nullptr;
    if(m_map!=nullptr)delete m_map;
    m_map=nullptr;
    if(m_bag!=nullptr)delete m_bag;
    m_bag=nullptr;
    snapshotStack.clear();
    emit exitLevel();
    return;
}