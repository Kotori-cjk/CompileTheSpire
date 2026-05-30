#ifndef MAPVIEW_H
#define MAPVIEW_H

#include "leveldata.h"

#include <QPixmap>
#include <QPoint>
#include <QSet>
#include <QWidget>

class MapView : public QWidget
{
    Q_OBJECT

public:
    explicit MapView(QWidget *parent = nullptr);

    void setLevel(const LevelData *level);
    void setPlayerPosition(const QPoint &position);
    void setClearedObjects(const QSet<QString> &openedChests, const QSet<QString> &defeatedMonsters);
    QSize minimumSizeHint() const override;

signals:
    void tileClicked(int row, int column);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QRect tileRect(int row, int column) const;
    QRect tileOuterRect(int row, int column) const;
    QRect mapBounds() const;
    int currentTileSize() const;
    QPoint tileAtPosition(const QPoint &position) const;
    QString monsterSpritePath(const QString &monsterId) const;
    bool isOpenTile(int row, int column) const;
    bool wallTouchesOpenTile(int row, int column) const;
    void drawTile(QPainter &painter, int row, int column, const QRect &rect, const QString &tileId) const;
    void drawObject(QPainter &painter, const QRect &rect, const QString &tileId) const;
    void drawChest(QPainter &painter, const QRect &rect) const;
    void drawClue(QPainter &painter, const QRect &rect) const;

    const LevelData *m_level = nullptr;
    QPoint m_playerPosition = QPoint(0, 0);
    QPoint m_selectedTile = QPoint(-1, -1);
    QSet<QString> m_openedChests;
    QSet<QString> m_defeatedMonsters;
    QPixmap m_playerPixmap;
    QPixmap m_bossPixmap;
    QPixmap m_chestPixmap;
    QPixmap m_cluePixmap;
    QPixmap m_referenceMap;
};

#endif // MAPVIEW_H
