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

    // --- chests state ---
    bool addBlockFromChest(const QString &blockId, const QString &chestId, QString* errorMsg = nullptr);
    bool chestIsEmpty(const QString &chestId) const;
    QSet<CodeBlock> blocksRemaining(const QString &chestId);
    QStringList remainingIds(const QString &chestId);


private:
    int m_capacity = 0;
    QVector<CodeBlock> bagBlocks;
    QMap<QString, QSet<CodeBlock>> leftBlocks;
};


#endif // INVENTORY_H
