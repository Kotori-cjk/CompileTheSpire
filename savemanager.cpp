#include "savemanager.h"
#include<QFile>
#include<QJsonDocument>
#include<QJsonObject>
#include<QJsonArray>

SaveManager::SaveManager():totalLevelCount(0){}
const QString SaveManager::SAVE_PATH="savedata/save.json";

void SaveManager::Init(int totalLevels){
    unlockState=QVector<int>(totalLevels,0);
    if (totalLevels>0){
        totalLevelCount=totalLevels;
        unlockState[0]=1;
    }
}

bool SaveManager::Save()const{
    QJsonObject save;
    save["totalLevelCount"]=totalLevelCount;
    QJsonArray unlocks;
    for(const auto& unlock:unlockState){
        unlocks.append(unlock);
    }
    save["unlockState"]=unlocks;
    QFile file(SAVE_PATH);
    if(!file.open(QIODevice::WriteOnly)){
        return false;
    }
    file.write(QJsonDocument(save).toJson());
    file.close();
    return true;
}

bool SaveManager::Load(int currentLevelCount){
    QFile file(SAVE_PATH);
    if(!file.open(QIODevice::ReadOnly)){
        return true;
    }
    QJsonParseError parseError;
    QJsonDocument doc=QJsonDocument::fromJson(file.readAll(),&parseError);
    file.close();
    if(parseError.error!=QJsonParseError::NoError){
        return false;
    }
    QJsonObject root=doc.object();
    totalLevelCount=std::max(root["totalLevelCount"].toInt(),totalLevelCount);
    QJsonArray unlocks=root["unlockState"].toArray();
    unlockState=QVector<int>();
    for(const auto& unlock:unlocks){
        unlockState.push_back(unlock.toBool());
    }
    for(int i=0;i<currentLevelCount-root["totalLevelCount"].toInt();i++){
        unlockState.push_back(0);
    }
    return true;
}

bool SaveManager::isUnlocked(int levelIndex)const{
    if(levelIndex>unlockState.size()||levelIndex>totalLevelCount){
        return false;
    }
    return unlockState[levelIndex];
}

bool SaveManager::Unlock(int levelIndex){
    if(levelIndex>unlockState.size()||levelIndex>totalLevelCount){
        return false;
    }
    if(unlockState[levelIndex]){
        return false;
    }
    unlockState[levelIndex]=1;
    return true;
}

void SaveManager::UnlockAll(){
    for(auto& i:unlockState){
        i=1;
    }
}

int SaveManager::levelCount(){
    return totalLevelCount;
}

void SaveManager::deleteSave(){
    QFile file(SAVE_PATH);
    file.remove();
}

void SaveManager::hardReset(){
    unlockState=QVector<int>(totalLevelCount,0);
    if(totalLevelCount>0)unlockState[0]=1;
    Save();
}