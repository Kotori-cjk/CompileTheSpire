#include "leveldata.h"
#include<QVector>
#include<QString>
#include<QMap>
#include<QJsonDocument>
#include<QJsonObject>
#include<QJsonArray>
#include<QFile>
#include<QDir>
#include<QPoint>

LevelData::LevelData() {}

Synthesis templateBreakdown(QString codeTemplate){
    Synthesis ret;
    ret.cluecnt=ret.spacecnt=0;
    QString s=codeTemplate,ss="";
    for(int i=0;i<s.length();i++){
        if(s[i]!='$'){
            ss+=s[i];
            continue;
        }
        ret.text.append(ss);
        ss="";
        SynthesisCell tmp;
        if(s[i+1]=='c')tmp.type="clue",ret.cluecnt++;
        else tmp.type="space",ret.spacecnt++;
        while(s[++i]!='$'){
            ss+=s[i];
        }
        tmp.id=ss;
        ret.cell.append(tmp);
        ss="";
    }
    ret.text.append(ss);
    return ret;
}

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
            tmpblock.type="natural";
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
        tmp.type=item["type"].toString();
        Synthesis synthesis=templateBreakdown(tmp.codeTemplate);
        for(int i=0;i<synthesis.cell.length();i++){
            if(synthesis.cell[i].type=="clue"){
                tmp.referencedClues.append(synthesis.cell[i].id);
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
        Clue tmp;
        tmp.val=root["clues"].toObject()[key].toString();
        this->clues[key]=tmp;
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
    Synthesis synthesis=templateBreakdown(boss.codeTemplate);
    for(int i=0;i<synthesis.cell.length();i++){
        if(synthesis.cell[i].type=="clue"){
            boss.referencedClues.append(synthesis.cell[i].id);
        }
    }
    const auto& g=root["map_grid"].toArray();
    for(const auto& item:g){
        const auto& row=item.toArray();
        QVector<QString>row_v;
        for(const auto& i:row){
            row_v.push_back(i.toString());
        }
        this->mapGrid.push_back(row_v);
    }
    QJsonArray startpos=root["start"].toArray();
    levelType=root["level_type"].toString();
    this->startpos=QPoint(startpos[0].toInt(),startpos[1].toInt());
    for(int i=0;i<mapHeight;i++){
        for(int j=0;j<mapWidth;j++){
            QString s=mapGrid[i][j];
            if(s.length()<4)continue;
            QString op=s.mid(0,3);
            if(op=="mon"){
                monsters[s].pos=QPoint(i,j);
            }
            if(op=="che"){
                chests[s].pos=QPoint(i,j);
            }
            if(op=="clu"){
                clues[s].pos=QPoint(i,j);
            }
            if(op=="bos"){
                boss.pos=QPoint(i,j);
            }
        }
    }
    this->boss=boss;
    this->monsters[boss.monsterId]=boss;
    return true;
}

QVector<LevelData> LoadDirectory(const QString& dirPath,QStringList* errors){
    QVector<LevelData>levels;
    QDir dir(dirPath);
    dir.setNameFilters({"*.json"});
    dir.setSorting(QDir::Name);
    const auto& files=dir.entryList(QDir::Files);
    for (const auto& file:files){
        QString filePath=dir.absoluteFilePath(file);
        QString errorMsg;
        LevelData data;
        if(data.LoadFromJson(filePath,&errorMsg)){
            levels.append(std::move(data));
        }
        else if(errors){
            errors->append(QString("%1: %2").arg(file,errorMsg));
        }
    }
    return levels;
}