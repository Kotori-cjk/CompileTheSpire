#ifndef COMBAT_H
#define COMBAT_H
#include "leveldata.h"
#include <QMap>
#include <QString>
#include <QStringList>

struct CombatResult{
    CodeBlock synthesizedBlock;
    QStringList usedBlocks;
    QStringList errorSpaces;
    QString resultType;//"count_error" "space_error" "success"
};

class Combat
{
public:
    Combat(const Monster& monster,const QMap<QString,Clue>& clues);
    Combat(const Boss& boss,const QMap<QString,Clue>& clues);

    bool isBoss()const;
    QString monsterId()const;
    QString monsterName()const;
    QString monsterNickname()const;
    QString monsterPic()const;
    QString codeTemplate()const;
    QStringList spaceIds()const;
    QStringList filledSpaces()const;
    QStringList unfilledSpaces()const;
    QMap<QString,CodeBlock> filledCodes()const;
    bool submitBlock(const Space& space,CodeBlock& blockId);
    bool isFilled(const QString& spaceId);
    bool isFilled(const Space& space);
    CombatResult submitCombat();
    Space id2Space(const QString& spaceId,bool* success=nullptr);

private:
    bool spaceValid(const Space& space);
    bool canSynthesize()const;
    CodeBlock synthesize(QStringList& used)const;
    const Monster* monster=nullptr;
    bool isBossVal=false;
    QMap<QString,CodeBlock>filledCodesMap;
    QMap<QString,Clue>clues;
};

#endif // COMBAT_H
