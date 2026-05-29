#include "combat.h"
#include<QRegularExpression>

Combat::Combat(const Monster& monster){
    this->monster=&monster;
    isBossVal=false;
}
Combat::Combat(const Boss& boss){
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
    QString type=space.type;
    QString blockCode=block.blockId;
    if(type=="regex"){
        for(const QString& pattern:space.values){
            QRegularExpression re(pattern);
            if(re.match(blockCode).hasMatch()){
                filledCodesMap[space.spaceId]=block;
                return true;
            }
        }
        return false;
    }
    if(type=="prefix"){
        for(const QString& prefix:space.values){
            if(blockCode.startsWith(prefix)){
                filledCodesMap[space.spaceId]=block;
                return true;
            }
        }
        return false;
    }
    if(type=="find"){
        for(const QString& substr:space.values){
            if(blockCode.contains(substr)){
                filledCodesMap[space.spaceId]=block;
                return true;
            }
        }
        return false;
    }
    if(type=="match"){
        for(const QString& match:space.values){
            if(blockCode==match){
                filledCodesMap[space.spaceId]=block;
                return true;
            }
        }
        return false;
    }
    return false;
}
CombatResult Combat::submitCombat(bool* success){
    if(!canSynthesize()){
        if(success!=nullptr)*success=false;
        return CombatResult();
    }
    CombatResult ret;
    ret.synthesizedBlock=synthesize(ret.usedBlocks);
    if(success!=nullptr)*success=true;
    return ret;
}
bool Combat::canSynthesize()const{
    return unfilledSpaces().size()==0;
}
CodeBlock Combat::synthesize(QStringList& used)const{
    if(!canSynthesize())return CodeBlock();
    CodeBlock ret;
    QStringList text,spaceId;
    QString s=monster->codeTemplate,ss;
    for(int i=0;i<s.length();i++){
        if(s[i]!='$'){
            ss+=s[i];
            continue;
        }
        text.append(ss);
        ss="";
        while(s[++i]!='$'){
            ss+=s[i];
        }
        spaceId.append(ss);
        ss="";
    }
    ret.blockId=monster->name;
    ret.blockId+='[';
    int cnt=0,size=filledCodesMap.size();
    for(const auto& i:spaceId){
        cnt++;
        CodeBlock b=filledCodesMap[i];
        ret.blockId+=b.blockId;
        used.append(b.blockId);
        if(cnt<size)ret.blockId+=',';
    }
    ret.blockId+=']';
    for(int i=0;i<text.length();i++){
        ret.code+=text[i];
        ret.code+=filledCodesMap[spaceId[i]].code;
    }
    ret.code+=ss;
    return ret;
}