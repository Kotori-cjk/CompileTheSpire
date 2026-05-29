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
    Inventory() = default;

    // --- bag state ---
    int capacity() const;
    void setCapacity(int capacity);
    int size() const;
    bool isEmpty() const;
    bool isFull() const;

    bool add(const CodeBlock &block);
    bool remove(const QString &blockId);
    bool contains(const QString &blockId) const;
    CodeBlock get(const QString &blockId) const;

    QVector<CodeBlock> all() const;
    QStringList ids() const;
    void clearBlocks();

    // --- chests state ---
    bool addBlockFromChest(const QString &blockId, const QString &chestId, QString* errorMsg = nullptr);
    bool chestIsEmpty(const QString &chestId) const;


private:
    int m_capacity = 0;
    QVector<CodeBlock> m_blocks;
    QMap<QString, QSet<QString>> leftBlocks;
};


#endif // INVENTORY_H
