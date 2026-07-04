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
    ret.defeatedCodes=m_map->defeatedCodes;
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
    m_map->defeatedCodes=snapshot.defeatedCodes;
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
        now.cleared=m_save.isCleared(i);
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
    if(m_locked||m_combat!=nullptr)return false;
    QString tileId=m_level->mapGrid[tarX][tarY];
    if(m_level->specialTags.contains("discard_drops")&&!(tileId=="boss"&&m_level->specialTags.contains("friendly_boss"))){
        if((tileId=="boss"||tileId.startsWith("mon"))&&!m_map->cleared[tarX][tarY]){
            int need=tileId=="boss"?m_level->boss.spaces.size():m_level->monsters[tileId].spaces.size();
            if(m_bag->bagSize()!=need){
                emit moveBlocked(QString("Need exactly %1 blocks, have %2").arg(need).arg(m_bag->bagSize()));
                return false;
            }
        }
    }
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
            if(m_level->chests[res.eventId].forcedPick){
                bool canTake;
                if(m_level->specialTags.contains("bundle_pick"))
                    canTake=m_bag->bagCapacity()-m_bag->bagSize()>=m_level->chests[res.eventId].blocks.size();
                else canTake=!m_bag->bagIsFull();
                if(canTake)m_locked=true;
                else if(m_level->specialTags.contains("skip_on_full"))snapshotStack.removeLast();
                else m_locked=true;
            }
            emit chestEntered(res.eventId,m_locked);
        }
        else if(res.event=="boss"&&m_level->specialTags.contains("friendly_boss")){
            QMap<QString,Clue>clues;
            if(m_combat!=nullptr)delete m_combat;
            m_combat=new Combat(m_level->boss,clues);
            emit combatStarted(res.eventId,m_combat);
        }
        else{
            const QString& monsterId=res.eventId;
            const Monster& monster=m_level->monsters[monsterId];
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
    if(m_combat->isFilled(spaceId)){
        const auto& map=m_combat->filledCodes();
        CodeBlock old=map[spaceId];
        m_bag->bagAdd(old);
    }
    m_bag->bagRemove(blockId);
    return m_combat->submitBlock(spc,tblock);
}
bool GameEngine::unfillSpace(const QString& spaceId){
    if(m_combat==nullptr||!m_combat->isFilled(spaceId))return false;
    CodeBlock block=m_combat->filledCodes()[spaceId];
    if(!m_combat->unfill(spaceId))return false;
    m_bag->bagAdd(block);
    return true;
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
bool GameEngine::takeBundleFromChest(const QString& chestId){
    if(!m_level->chests.contains(chestId))return false;
    const Chest& chest=m_level->chests[chestId];
    int need=chest.blocks.size();
    if(m_bag->bagCapacity()-m_bag->bagSize()<need)return false;
    for(const auto& key:chest.blocks.keys()){
        QString msg;
        if(!m_bag->addBlockFromChest(key,chestId,&msg))return false;
    }
    m_locked=false;
    m_map->Clear(chest.pos);
    snapshotStack.append(getCurrentSnapshot());
    return true;
}
bool GameEngine::exitChest(const QString& chestId){
    m_locked=false;
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
    m_locked=false;
    if(!m_level->chests[chestId].repeat||m_bag->chestIsEmpty(chestId)){
        m_map->Clear(m_level->chests[chestId].pos);
    }
    snapshotStack.append(getCurrentSnapshot());
    return true;
}
bool GameEngine::exitCombat(){
    if(m_combat!=nullptr){
        for(const auto& block:m_combat->filledCodes()){
            m_bag->bagAdd(block);
        }
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
            m_save.Clear(m_level->levelIndex);
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
            bool discardDrop=false;
            if(m_level->specialTags.contains("discard_drops"))
                discardDrop=true;
            if(!discardDrop)m_bag->bagAdd(result.synthesizedBlock);
            m_map->setDefeatedCode(m_combat->monsterId(),result.synthesizedBlock.code);
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
bool GameEngine::discardBlock(const QString& blockId){
    if(m_bag==nullptr||!m_bag->bagContains(blockId))return false;
    if(m_level->specialTags.contains("no_discard"))return false;
    if(m_level->specialTags.contains("no_natural_discard")&&m_bag->bagGet(blockId).type=="natural")return false;
    m_bag->bagRemove(blockId);
    snapshotStack.append(getCurrentSnapshot());
    return true;
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
    QPoint pos=m_map->playerPos();
    emit forcedMove(pos);
    QString event=m_map->getEvent(pos);
    if(event=="chest"){
        m_locked=false;
        const Chest& chest=m_level->chests[m_level->mapGrid[pos.x()][pos.y()]];
        if(chest.forcedPick){
            bool canTake;
            if(m_level->specialTags.contains("bundle_pick"))
                canTake=m_bag->bagCapacity()-m_bag->bagSize()>=chest.blocks.size();
            else canTake=!m_bag->bagIsFull();
            if(canTake)m_locked=true;
            else if(!m_level->specialTags.contains("skip_on_full"))m_locked=true;
        }
        emit chestEntered(m_level->mapGrid[pos.x()][pos.y()],m_locked);
    }
    else if(event=="clue"){
        QString clueId=m_level->mapGrid[pos.x()][pos.y()];
        for(const auto& monsterId:m_level->monsters.keys()){
            const auto& mcd=m_map->getMonsterClueDetail(monsterId);
            if(mcd.clueUnlockStates.contains(clueId)){
                emit clueRevealed(clueId,monsterId,m_level->clues[clueId].val);
                break;
            }
        }
    }
    else if(event=="monster"){
        const QString& monsterId=m_level->mapGrid[pos.x()][pos.y()];
        const Monster& monster=m_level->monsters[monsterId];
        auto mcd=m_map->getMonsterClueDetail(monsterId);
        QMap<QString,Clue> clues;
        for(const auto& clueId:mcd.clueIds){
            clues[clueId]=m_level->clues[clueId];
        }
        if(m_combat!=nullptr)delete m_combat;
        m_combat=new Combat(monster,clues);
        emit combatStarted(monsterId,m_combat);
    }
    else if(event=="boss"){
        auto mcd=m_map->getMonsterClueDetail(m_level->boss.monsterId);
        QMap<QString,Clue> clues;
        for(const auto& clueId:mcd.clueIds){
            clues[clueId]=m_level->clues[clueId];
        }
        if(m_combat!=nullptr)delete m_combat;
        m_combat=new Combat(m_level->boss,clues);
        emit combatStarted(m_level->boss.monsterId,m_combat);
    }
    return true;
}
bool GameEngine::resetLevel(){
    if(snapshotStack.isEmpty())return false;
    m_locked=false;
    restoreFromSnapshot(snapshotStack[0]);
    snapshotStack=QVector<GameSnapshot>();
    snapshotStack.append(getCurrentSnapshot());
    QPoint pos=m_map->playerPos();
    emit forcedMove(pos);
    QString event=m_map->getEvent(pos);
    if(event=="chest"){
        const Chest& chest=m_level->chests[m_level->mapGrid[pos.x()][pos.y()]];
        if(chest.forcedPick){
            bool canTake;
            if(m_level->specialTags.contains("bundle_pick"))
                canTake=m_bag->bagCapacity()-m_bag->bagSize()>=chest.blocks.size();
            else canTake=!m_bag->bagIsFull();
            if(canTake)m_locked=true;
            else if(!m_level->specialTags.contains("skip_on_full"))m_locked=true;
        }
        emit chestEntered(m_level->mapGrid[pos.x()][pos.y()],m_locked);
    }
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