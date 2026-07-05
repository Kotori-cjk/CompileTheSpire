#ifndef MAPVIEW_H
#define MAPVIEW_H

#include "leveldata.h"

#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QSet>
#include <QTimer>
#include <QWidget>

class MapView : public QWidget
{
    Q_OBJECT

public:
    explicit MapView(QWidget *parent = nullptr);

    void setLevel(const LevelData *level);
    void setPlayerPosition(const QPoint &position);
    void setClearedObjects(const QSet<QString> &openedChests, const QSet<QString> &defeatedMonsters);
    void setCollectedClues(const QSet<QString> &collectedClues);
    void setBlockedTiles(const QSet<QPoint> &blocked);
    void setMovePath(const QVector<QPoint> &path, int consumedCount);
    QSize minimumSizeHint() const override;
    bool isPlayerAnimating() const;

signals:
    void tileClicked(int row, int column);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QRect tileRect(int row, int column) const;
    QRect visualTileRect(const QPointF &position) const;
    QRect tileOuterRect(int row, int column) const;
    QRect mapBounds() const;
    int currentTileSize() const;
    QPoint tileAtPosition(const QPoint &position) const;
    QString spritePathForPic(const QString &pic) const;
    QString monsterSpritePath(const QString &monsterId) const;
    bool isOpenTile(int row, int column) const;
    bool wallTouchesOpenTile(int row, int column) const;
    void drawTile(QPainter &painter, int row, int column, const QRect &rect, const QString &tileId) const;
    void drawMovePath(QPainter &painter) const;
    void drawObject(QPainter &painter, const QRect &rect, const QString &tileId) const;
    void drawChest(QPainter &painter, const QRect &rect, const QString &tileId) const;
    void drawClue(QPainter &painter, const QRect &rect) const;
    void drawPlayer(QPainter &painter) const;
    QPixmap currentPlayerFrame() const;

    const LevelData *m_level = nullptr;
    QPoint m_playerPosition = QPoint(0, 0);
    QPointF m_playerVisualPosition = QPointF(0, 0);
    QPointF m_playerAnimationStart = QPointF(0, 0);
    QPointF m_playerAnimationEnd = QPointF(0, 0);
    QPoint m_selectedTile = QPoint(-1, -1);
    QSet<QString> m_openedChests;
    QSet<QString> m_defeatedMonsters;
    QSet<QString> m_collectedClues;
    QSet<QPoint> m_blockedTiles;
    QVector<QPoint> m_movePath;
    int m_consumedPathCount = 0;
    QTimer m_playerAnimationTimer;
    qreal m_playerAnimationProgress = 1.0;
    int m_playerFrameIndex = 0;
    int m_playerFrameTick = 0;
    int m_playerFacing = 0;
    bool m_playerWalking = false;
    QPixmap m_playerPixmap;
    QVector<QVector<QPixmap>> m_playerFrames;
    QPixmap m_bossPixmap;
    QPixmap m_forcedChestPixmap;
    QPixmap m_unforcedChestPixmap;
    QPixmap m_cluePixmap;
    QPixmap m_referenceMap;
    QPixmap m_wallPixmap;
};

#endif // MAPVIEW_H
