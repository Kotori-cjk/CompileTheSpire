#include "combat.h"
#include<QRegularExpression>

Combat::Combat(const Monster& monster,const QMap<QString,Clue>& clues){
    this->clues=clues;
    this->monster=&monster;
    isBossVal=false;
}
Combat::Combat(const Boss& boss,const QMap<QString,Clue>& clues){
    this->clues=clues;
    isBossVal=true;
    this->monster=&boss;
}
bool Combat::isBoss()const{
    return isBossVal;
}
QString Combat::monsterId()const{
    return monster->monsterId;
}
QString Combat::monsterName()const{
    return monster->name;
}
QString Combat::monsterNickname()const{
    return monster->nickname;
}
QString Combat::monsterPic()const{
    return monster->pic;
}
QString Combat::codeTemplate()const{
    return monster->codeTemplate;
}
QStringList Combat::spaceIds()const{
    QStringList ret;
    for(const auto& i:monster->spaces){
        ret.append(i.spaceId);
    }
    return ret;
}
QStringList Combat::filledSpaces()const{
    return filledCodesMap.keys();
}
QStringList Combat::unfilledSpaces()const{
    QStringList ret;
    for(const auto& i:monster->spaces){
        QString id=i.spaceId;
        if(!filledCodesMap.contains(id)){
            ret.append(id);
        }
    }
    return ret;
}
QMap<QString,CodeBlock> Combat::filledCodes()const{
    return filledCodesMap;
}
bool Combat::submitBlock(const Space& space,CodeBlock& block){
    if(!spaceIds().contains(space.spaceId)){
        return false;
    }
    filledCodesMap[space.spaceId]=block;
    return true;
}
bool Combat::unfill(const QString& spaceId){
    return filledCodesMap.remove(spaceId);
}
bool Combat::isFilled(const QString& spaceId){
    return filledCodesMap.contains(spaceId);
}
bool Combat::isFilled(const Space& space){
    return isFilled(space.spaceId);
}
CombatResult Combat::submitCombat(){
    CombatResult ret;
    if(!canSynthesize()){
        ret.resultType="count_error";
        return ret;
    }
    for(const auto& spaceId:filledCodesMap.keys()){
        if(!spaceValid(id2Space(spaceId))){
            ret.resultType="space_error";
            ret.errorSpaces.append(spaceId);
        }
    }
    if(ret.resultType=="space_error"){
        return ret;
    }
    ret.resultType="success";
    ret.synthesizedBlock=synthesize(ret.usedBlocks);
    return ret;
}
Space Combat::id2Space(const QString& spaceId,bool* success){
    for(const auto& i:monster->spaces){
        if(spaceId==i.spaceId){
            if(success!=nullptr)*success=true;
            return i;
        }
    }
    if(success!=nullptr)*success=false;
    return Space();
}
bool Combat::spaceValid(const Space& space){
    if(!isFilled(space))return false;
    QString type=space.type;
    CodeBlock block=filledCodesMap[space.spaceId];
    QString blockCode=block.blockId;
    if(type=="regex"){
        for(const QString& pattern:space.values){
            QRegularExpression re(pattern);
            if(re.match(blockCode).hasMatch()){
                return true;
            }
        }
        return false;
    }
    if(type=="prefix"){
        for(const QString& prefix:space.values){
            if(blockCode.startsWith(prefix)){
                return true;
            }
        }
        return false;
    }
    if(type=="find"){
        for(const QString& substr:space.values){
            if(blockCode.contains(substr)){
                return true;
            }
        }
        return false;
    }
    if(type=="match"){
        for(const QString& match:space.values){
            if(blockCode==match){
                return true;
            }
        }
        return false;
    }
    return false;
}
bool Combat::canSynthesize()const{
    return unfilledSpaces().size()==0;
}
CodeBlock Combat::synthesize(QStringList& used)const{
    if(!canSynthesize())return CodeBlock();
    CodeBlock ret;
    Synthesis synthesis=templateBreakdown(monster->codeTemplate);
    ret.blockId=monster->name;
    ret.blockId+='[';
    int cnt=0,size=synthesis.spacecnt;
    for(const auto& i:synthesis.cell){
        if(i.type=="clue")continue;
        cnt++;
        CodeBlock b=filledCodesMap[i.id];
        ret.blockId+=b.blockId;
        used.append(b.blockId);
        if(cnt<size)ret.blockId+=',';
    }
    ret.blockId+=']';
    for(int i=0;i<synthesis.text.length();i++){
        ret.code+=synthesis.text[i];
        if(i<synthesis.cell.length()&&synthesis.cell[i].type=="clue"){
            ret.code+=clues[synthesis.cell[i].id].val;
        }
        if(i<synthesis.cell.length()&&synthesis.cell[i].type=="space"){
            ret.code+=filledCodesMap[synthesis.cell[i].id].code;
        }
    }
    ret.type=monster->type;
    return ret;
}