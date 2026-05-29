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

    // --- chest state ---
    bool isChestOpened(const QString &chestId) const;
    void markChestOpened(const QString &chestId);
    bool isBlockEverTaken(const QString &chestId, const QString &blockId) const;
    void markBlockTaken(const QString &chestId, const QString &blockId);
    void clearChestStates();

    // clear all
    void clear();


private:
    int m_capacity = 0;
    QVector<CodeBlock> m_blocks;
    QSet<QString> openedChests;
    QMap<QString, QSet<QString>> takenBlocks;
};

#endif // INVENTORY_H
