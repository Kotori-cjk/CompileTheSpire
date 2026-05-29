#include "inventory.h"

//---init---

Inventory::Inventory(const LevelData* ptr){
    if(ptr == nullptr){
        return;
    }
    m_capacity = ptr->bagSize;
    bagBlocks = QVector<CodeBlock>();
    for(auto it = ptr->chests.constBegin(); it != ptr->chests.constEnd(); ++it){
        auto chestId = it.key();
        auto chest = it.value();
        leftBlocks[chestId] = QSet<CodeBlock>();
        for(auto it2 = chest.blocks.constBegin(); it2 != chest.blocks.constEnd(); ++it2){
            leftBlocks[chestId].insert(it2.value());
        }
    }
}

//---bag state---

int Inventory::bagCapacity()const {
    return m_capacity;
}

int Inventory::bagSize()const {
    return bagBlocks.size();
}

bool Inventory::bagIsEmpty()const {
    return bagBlocks.isEmpty();
}

bool Inventory::bagIsFull()const {
    return m_capacity <= bagBlocks.size();
}

bool Inventory::bagAdd(const CodeBlock &block) {
    if(bagIsFull()){
        return false;
    }
    bagBlocks.append(block);
    return true;
}

bool Inventory::bagRemove(const QString &blockId){
    for(int i = 0; i < bagBlocks.size(); ++i){
        if(bagBlocks[i].blockId == blockId){
            bagBlocks.removeAt(i);
            return true;
        }
    }
    return false;
}

bool Inventory::bagContains(const QString &blockId)const {
    for(const CodeBlock &block : bagBlocks){
        if(block.blockId == blockId){
            return true;
        }
    }
    return false;
}

CodeBlock Inventory::bagGet(const QString& blockId,bool* success)const{
    for(const auto& i:bagBlocks){
        if(blockId==i.blockId){
            if(success!=nullptr)*success=true;
            return i;
        }
    }
    if(success!=nullptr)*success=false;
    return CodeBlock();
}

QVector<CodeBlock> Inventory::bag()const {
    return bagBlocks;
}

QStringList Inventory::bagIds()const {
    QStringList ids;
    for(const auto& block : bagBlocks){
        ids.append(block.blockId);
    }
    return ids;
}

void Inventory::clearBag() {
    bagBlocks.clear();
}

//---chest state---

bool Inventory::addBlockFromChest(const QString &blockId, const QString &chestId, QString* errorMsg) {
    if(bagIsFull()){
        if(errorMsg != nullptr){
            *errorMsg = "Bag is full";
        }
        return false;
    }
    for(const auto &block : leftBlocks[chestId]){
        if(block.blockId == blockId){
            bagAdd(block);
            leftBlocks[chestId].remove(block);
            return true;
        }
    }
    if(errorMsg != nullptr){
        *errorMsg = "Block is not in chest";
    }
    return false;
}

bool Inventory::chestIsEmpty(const QString &chestId)const {
    return leftBlocks[chestId].empty();
}

QSet<CodeBlock> Inventory::blocksRemaining(const QString &chestId) {
    return leftBlocks[chestId];
}

QStringList Inventory::remainingIds(const QString &chestId){
    QStringList ids;
    for(const auto &block : leftBlocks[chestId]){
        ids.append(block.blockId);
    }
    return ids;
}