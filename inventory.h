#ifndef INVENTORY_H
#define INVENTORY_H

#include "leveldata.h"

#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class Inventory
{
public:
    Inventory(const LevelData* ptr);

    // --- bag state ---
    int bagCapacity() const;
    int bagSize() const;
    bool bagIsEmpty() const;
    bool bagIsFull() const;

    bool bagAdd(const CodeBlock &block);
    bool bagRemove(const QString &blockId);
    bool bagContains(const QString &blockId) const;

    QVector<CodeBlock> bag() const;
    QStringList bagIds() const;
    void clearBag();

    CodeBlock bagGet(const QString& blockId,bool* success=nullptr)const;

    // --- chests state ---
    bool addBlockFromChest(const QString &blockId, const QString &chestId, QString* errorMsg = nullptr);
    bool chestIsEmpty(const QString &chestId) const;
    QVector<CodeBlock> blocksRemaining(const QString &chestId);
    QStringList remainingIds(const QString &chestId);
    QMap<QString, QSet<QString>> leftBlocks;
    QVector<CodeBlock> bagBlocks;

private:
    const LevelData* leveldata;
    int m_capacity = 0;


};


#endif // INVENTORY_H