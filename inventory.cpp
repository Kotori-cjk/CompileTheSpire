#include "inventory.h"

// --- Code blocks (bag) ---

int Inventory::capacity() const
{
    return m_capacity;
}

void Inventory::setCapacity(int capacity)
{
    m_capacity = capacity;
}

int Inventory::size() const
{
    return m_blocks.size();
}

bool Inventory::isEmpty() const
{
    return m_blocks.isEmpty();
}

bool Inventory::isFull() const
{
    return m_capacity > 0 && m_blocks.size() >= m_capacity;
}

bool Inventory::add(const CodeBlock &block)
{
    if (contains(block.blockId)) {
        return false;
    }
    if (isFull()) {
        return false;
    }
    m_blocks.append(block);
    return true;
}

bool Inventory::remove(const QString &blockId)
{
    for (int i = 0; i < m_blocks.size(); ++i) {
        if (m_blocks[i].blockId == blockId) {
            m_blocks.removeAt(i);
            return true;
        }
    }
    return false;
}

bool Inventory::contains(const QString &blockId) const
{
    for (const CodeBlock &block : m_blocks) {
        if (block.blockId == blockId) {
            return true;
        }
    }
    return false;
}

CodeBlock Inventory::get(const QString &blockId) const
{
    for (const CodeBlock &block : m_blocks) {
        if (block.blockId == blockId) {
            return block;
        }
    }
    return {};
}

QVector<CodeBlock> Inventory::all() const
{
    return m_blocks;
}

QStringList Inventory::ids() const
{
    QStringList result;
    for (const CodeBlock &block : m_blocks) {
        result.append(block.blockId);
    }
    return result;
}

void Inventory::clearBlocks()
{
    m_blocks.clear();
}

// --- Chest state ---

bool Inventory::isChestOpened(const QString &chestId) const
{
    return openedChests.contains(chestId);
}

void Inventory::markChestOpened(const QString &chestId)
{
    openedChests.insert(chestId);
}

bool Inventory::isBlockEverTaken(const QString &chestId, const QString &blockId) const
{
    return takenBlocks.value(chestId).contains(blockId);
}

void Inventory::markBlockTaken(const QString &chestId, const QString &blockId)
{
    takenBlocks[chestId].insert(blockId);
}

void Inventory::clear(){
    Inventory::clearBlocks();
    Inventory::clearChestStates();
}