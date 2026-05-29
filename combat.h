#ifndef COMBAT_H
#define COMBAT_H
#include "leveldata.h"
#include <QMap>
#include <QString>
#include <QStringList>

struct CombatResult{
    CodeBlock synthesizedBlock;
    QStringList usedBlocks;
};

class Combat
{
public:
    Combat(const Monster& monster);
    Combat(const Boss& boss);

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
    CombatResult submitCombat(bool* success=nullptr);

private:
    bool canSynthesize()const;
    CodeBlock synthesize(QStringList& used)const;
    const Monster* monster=nullptr;
    bool isBossVal=false;
    QMap<QString,CodeBlock>filledCodesMap;
};

#endif // COMBAT_H
