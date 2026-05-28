#include "leveldata.h"
#include<QVector>
#include<QString>
#include<QMap>
#include<QJsonDocument>
#include<QJsonObject>
#include<QJsonArray>
#include<QFile>

LevelData::LevelData() {}

QVector<Space> LevelData::parseSpaces(const QJsonObject &spacesObj){
    QVector<Space>ret;
    for(const auto& key:spacesObj.keys()){
        QJsonObject item=spacesObj[key].toObject();
        Space tmp;
        tmp.type=item["type"].toString();
        tmp.spaceId=key;
        QJsonArray arr=item["val"].toArray();
        for(const auto i:arr){
            tmp.values.append(i.toString());
        }
        ret.push_back(tmp);
    }
    return ret;
}

QMap<QString,Chest> LevelData::parseChests(const QJsonObject &chestsObj){
    QMap<QString,Chest>ret;
    for(const auto& key:chestsObj.keys()){
        QJsonObject item=chestsObj[key].toObject();
        Chest tmp;
        tmp.chestId=key;
        tmp.repeat=item["repeat"].toBool();
        tmp.forcedPick=item["forced_pick"].toBool();
        QMap<QString,CodeBlock>blocks;
        QJsonObject blocks_j=item["blocks"].toObject();
        for(const auto& BlockId:blocks_j.keys()){
            CodeBlock tmpblock;
            tmpblock.blockId=BlockId;
            tmpblock.code=blocks_j[BlockId].toString();
            blocks[BlockId]=tmpblock;
        }
        tmp.blocks=blocks;
        ret[key]=tmp;
    }
    return ret;
}

QMap<QString,Monster> LevelData::parseMonsters(const QJsonObject &monstersObj){
    QMap<QString,Monster>ret;
    for(const auto& key:monstersObj.keys()){
        QJsonObject item=monstersObj[key].toObject();
        Monster tmp;
        tmp.monsterId=key;
        tmp.name=item["name"].toString();
        tmp.nickname=item["nickname"].toString();
        tmp.pic=item["pic"].toString();
        tmp.spaces=parseSpaces(item["spaces"].toObject());
        tmp.codeTemplate=item["code"].toString();
        QString s=tmp.codeTemplate;
        for(int i=0;i<tmp.codeTemplate.length();i++){
            if(s[i]=='$'&&i+1<s.length()&&s[i+1]=='c'){
                QString ss="";
                while(s[++i]!='$'){
                    ss+=s[i];
                }
                tmp.referencedClues.append(ss);
            }
        }
        ret[key]=tmp;
    }
    return ret;
}

bool LevelData::LoadFromJson(const QString& filePath,QString* errorMessage){
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly)){
        if(errorMessage){
            *errorMessage=QString("Cannot open: %1").arg(filePath);
        }
        return false;
    }
    QJsonParseError parseError;
    QJsonDocument doc=QJsonDocument::fromJson(file.readAll(),&parseError);
    file.close();
    if(parseError.error!=QJsonParseError::NoError){
        if(errorMessage){
            *errorMessage=QString("JSON error: %1").arg(parseError.errorString());
        }
        return false;
    }
    QJsonObject root=doc.object();
    this->mapWidth=root["map_width"].toInt();
    this->mapHeight=root["map_height"].toInt();
    this->bagSize=root["bag_size"].toInt();
    this->endText=root["end_text"].toString();
    for(const auto& s:root["special_tags"].toArray()){
        this->specialTags.append(s.toString());
    }
    for(const auto& key:root["clues"].toObject().keys()){
        this->clues[key]=root["clues"].toObject()[key].toString();
    }
    this->monsters=this->parseMonsters(root["monsters"].toObject());
    this->chests=this->parseChests(root["chests"].toObject());
    Boss boss;
    QJsonObject boss_j=root["boss"].toObject();
    boss.input=boss_j["input"].toString();
    boss.output=boss_j["output"].toString();
    boss.codeTemplate=boss_j["code"].toString();
    boss.monsterId="boss";
    boss.name="boss";
    boss.nickname=boss_j["nickname"].toString();
    boss.pic=boss_j["pic"].toString();
    boss.spaces=this->parseSpaces(boss_j["spaces"].toObject());
    boss.type="boss";
    QString s=boss.codeTemplate;
    for(int i=0;i<s.length();i++){
        if(s[i]=='$'&&i+1<s.length()&&s[i+1]=='c'){
            QString ss="";
            while(s[++i]!='$'){
                ss+=s[i];
            }
            boss.referencedClues.append(ss);
        }
    }
    this->boss=boss;
    const auto& g=root["map_grid"].toArray();
    for(const auto& item:g){
        const auto& row=item.toArray();
        QVector<QString>row_v;
        for(const auto& i:row){
            row_v.push_back(i.toString());
        }
        this->mapGrid.push_back(row_v);
    }
    return true;
}