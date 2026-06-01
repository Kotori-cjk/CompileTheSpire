#include "mapview.h"

#include <QImage>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

#include <algorithm>

namespace {
constexpr int tilePadding = 3;
constexpr int mapMargin = 32;

int stableNoise(int row, int column, int salt)
{
    int value = row * 73856093 ^ column * 19349663 ^ salt * 83492791;
    value ^= value >> 13;
    value *= 1274126177;
    return qAbs(value);
}

QColor shiftedColor(const QColor &base, int shift)
{
    return QColor(qBound(0, base.red() + shift, 255),
                  qBound(0, base.green() + shift, 255),
                  qBound(0, base.blue() + shift, 255),
                  base.alpha());
}

QPixmap loadSprite(const QString &path, bool removeBlackBackground = false)
{
    QImage image(path);
    if (image.isNull()) {
        return {};
    }

    image = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            QColor color = QColor::fromRgba(line[x]);
            if (color.alpha() < 8 || (removeBlackBackground && color.red() < 12 && color.green() < 12 && color.blue() < 12)) {
                line[x] = qRgba(color.red(), color.green(), color.blue(), 0);
            }
        }
    }

    QRect crop;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (QColor::fromRgba(image.pixel(x, y)).alpha() > 8) {
                crop = crop.isNull() ? QRect(x, y, 1, 1) : crop.united(QRect(x, y, 1, 1));
            }
        }
    }

    if (!crop.isNull()) {
        image = image.copy(crop.adjusted(-4, -4, 4, 4).intersected(image.rect()));
    }

    return QPixmap::fromImage(image);
}

QVector<QVector<QPixmap>> loadSpriteSheet(const QString &path, int rows, int columns)
{
    QImage image(path);
    if (image.isNull() || rows <= 0 || columns <= 0) {
        return {};
    }

    image = image.convertToFormat(QImage::Format_ARGB32);
    auto clearBakedCheckerBackground = [](QImage &frame) {
        for (int y = 0; y < frame.height(); ++y) {
            QRgb *scan = reinterpret_cast<QRgb *>(frame.scanLine(y));
            for (int x = 0; x < frame.width(); ++x) {
                QColor color = QColor::fromRgba(scan[x]);
                const int maxChannel = qMax(color.red(), qMax(color.green(), color.blue()));
                const int minChannel = qMin(color.red(), qMin(color.green(), color.blue()));
                const bool likelyCheckerBackground = maxChannel > 218 && (maxChannel - minChannel) < 24;
                if (color.alpha() < 8 || likelyCheckerBackground) {
                    scan[x] = qRgba(color.red(), color.green(), color.blue(), 0);
                }
            }
        }
    };
    auto croppedFrame = [](const QImage &source, const QRect &bounds) {
        return QPixmap::fromImage(source.copy(bounds.adjusted(-3, -3, 3, 3).intersected(source.rect())));
    };

    clearBakedCheckerBackground(image);

    const int width = image.width();
    const int height = image.height();
    QVector<uchar> visited(width * height, 0);
    QVector<QRect> components;
    components.reserve(rows * columns);

    auto opaque = [&image, width, height](int x, int y) {
        return x >= 0 && x < width && y >= 0 && y < height && QColor::fromRgba(image.pixel(x, y)).alpha() > 8;
    };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int startIndex = y * width + x;
            if (visited[startIndex] || !opaque(x, y)) {
                continue;
            }

            QRect bounds(x, y, 1, 1);
            QVector<QPoint> stack;
            stack.append(QPoint(x, y));
            visited[startIndex] = 1;
            int area = 0;

            while (!stack.isEmpty()) {
                const QPoint point = stack.takeLast();
                ++area;
                bounds = bounds.united(QRect(point, QSize(1, 1)));
                const QPoint neighbors[] = {
                    QPoint(point.x() + 1, point.y()),
                    QPoint(point.x() - 1, point.y()),
                    QPoint(point.x(), point.y() + 1),
                    QPoint(point.x(), point.y() - 1)
                };
                for (const QPoint &next : neighbors) {
                    if (!opaque(next.x(), next.y())) {
                        continue;
                    }
                    const int index = next.y() * width + next.x();
                    if (visited[index]) {
                        continue;
                    }
                    visited[index] = 1;
                    stack.append(next);
                }
            }

            if (area > 350 && bounds.width() > 18 && bounds.height() > 24) {
                components.append(bounds);
            }
        }
    }

    if (components.size() >= rows * columns) {
        std::sort(components.begin(), components.end(), [](const QRect &a, const QRect &b) {
            if (qAbs(a.center().y() - b.center().y()) > 60) {
                return a.center().y() < b.center().y();
            }
            return a.center().x() < b.center().x();
        });
        components = components.mid(0, rows * columns);

        QVector<QVector<QPixmap>> frames;
        frames.reserve(rows);
        for (int row = 0; row < rows; ++row) {
            QVector<QRect> lineRects = components.mid(row * columns, columns);
            std::sort(lineRects.begin(), lineRects.end(), [](const QRect &a, const QRect &b) {
                return a.center().x() < b.center().x();
            });

            QVector<QPixmap> line;
            line.reserve(columns);
            for (const QRect &rect : lineRects) {
                line.append(croppedFrame(image, rect));
            }
            frames.append(line);
        }
        return frames;
    }

    const qreal cellWidth = static_cast<qreal>(image.width()) / columns;
    const qreal cellHeight = static_cast<qreal>(image.height()) / rows;

    QVector<QVector<QPixmap>> frames;
    frames.reserve(rows);
    for (int row = 0; row < rows; ++row) {
        QVector<QPixmap> line;
        line.reserve(columns);
        for (int column = 0; column < columns; ++column) {
            const QRect cell(qRound(column * cellWidth),
                             qRound(row * cellHeight),
                             qRound((column + 1) * cellWidth) - qRound(column * cellWidth),
                             qRound((row + 1) * cellHeight) - qRound(row * cellHeight));
            QImage frame = image.copy(cell).convertToFormat(QImage::Format_ARGB32);
            clearBakedCheckerBackground(frame);

            QRect crop;
            for (int y = 0; y < frame.height(); ++y) {
                for (int x = 0; x < frame.width(); ++x) {
                    if (QColor::fromRgba(frame.pixel(x, y)).alpha() > 8) {
                        crop = crop.isNull() ? QRect(x, y, 1, 1) : crop.united(QRect(x, y, 1, 1));
                    }
                }
            }
            if (!crop.isNull()) {
                frame = frame.copy(crop.adjusted(-3, -3, 3, 3).intersected(frame.rect()));
            }
            line.append(QPixmap::fromImage(frame));
        }
        frames.append(line);
    }
    return frames;
}

QColor floorColor(int row, int column)
{
    const int tone = 54 + ((row * 17 + column * 11) % 26);
    return QColor(tone, tone, tone - 8);
}

QColor wallColor(int row, int column)
{
    const int tone = 24 + ((row * 13 + column * 7) % 20);
    return QColor(tone, tone, tone + 3);
}

QRect spriteRectInTile(const QRect &tileRect, const QPixmap &pixmap, int widthPercent, int heightPercent, int yOffsetPercent)
{
    if (pixmap.isNull()) {
        return tileRect;
    }

    const QSize targetLimit(tileRect.width() * widthPercent / 100, tileRect.height() * heightPercent / 100);
    QSize spriteSize = pixmap.size();
    spriteSize.scale(targetLimit, Qt::KeepAspectRatio);
    const int x = tileRect.center().x() - spriteSize.width() / 2;
    const int y = tileRect.bottom() - spriteSize.height() + tileRect.height() * yOffsetPercent / 100;
    return QRect(QPoint(x, y), spriteSize);
}
}

MapView::MapView(QWidget *parent)
    : QWidget(parent)
    , m_playerPixmap(loadSprite(":/images/assets/jibao.png", true))
    , m_playerFrames(loadSpriteSheet(":/images/assets/jibao_walk_sheet.png", 8, 4))
    , m_bossPixmap(loadSprite(":/images/assets/marry_ann.png"))
    , m_forcedChestPixmap(loadSprite(":/images/assets/forced_chest.png", true))
    , m_unforcedChestPixmap(loadSprite(":/images/assets/unforce_chest.png", true))
    , m_cluePixmap(loadSprite(":/images/assets/clue.png"))
    , m_referenceMap(":/images/assets/example_map.png")
    , m_wallPixmap(":/images/assets/wall_image.jpg")
{
    setMouseTracking(true);
    setMinimumSize(560, 420);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_playerAnimationTimer.setInterval(16);
    connect(&m_playerAnimationTimer, &QTimer::timeout, this, [this]() {
        m_playerAnimationProgress = qMin<qreal>(1.0, m_playerAnimationProgress + 0.16);
        const qreal eased = 1.0 - qPow(1.0 - m_playerAnimationProgress, 2.0);
        m_playerVisualPosition = m_playerAnimationStart + (m_playerAnimationEnd - m_playerAnimationStart) * eased;
        ++m_playerFrameTick;
        if (m_playerFrameTick % 4 == 0) {
            m_playerFrameIndex = (m_playerFrameIndex + 1) % 4;
        }
        if (m_playerAnimationProgress >= 1.0) {
            m_playerVisualPosition = m_playerAnimationEnd;
            m_playerWalking = false;
            m_playerAnimationTimer.stop();
        }
        update();
    });
}

void MapView::setLevel(const LevelData *level)
{
    m_level = level;
    m_selectedTile = QPoint(-1, -1);
    update();
}

void MapView::setPlayerPosition(const QPoint &position)
{
    const QPoint previous = m_playerPosition;
    m_playerPosition = position;
    const QPointF target(position.x(), position.y());
    if (m_playerVisualPosition == QPointF(0, 0) && previous == QPoint(0, 0)) {
        m_playerVisualPosition = target;
    } else if (m_playerVisualPosition != target) {
        const QPoint delta = position - previous;
        if (qAbs(delta.x()) > qAbs(delta.y())) {
            m_playerFacing = delta.x() >= 0 ? 3 : 2;
        } else if (delta.y() != 0) {
            m_playerFacing = delta.y() >= 0 ? 0 : 1;
        }
        m_playerAnimationStart = m_playerVisualPosition;
        m_playerAnimationEnd = target;
        m_playerAnimationProgress = 0.0;
        m_playerFrameTick = 0;
        m_playerWalking = true;
        if (!m_playerAnimationTimer.isActive()) {
            m_playerAnimationTimer.start();
        }
    }
    update();
}

void MapView::setClearedObjects(const QSet<QString> &openedChests, const QSet<QString> &defeatedMonsters)
{
    m_openedChests = openedChests;
    m_defeatedMonsters = defeatedMonsters;
    update();
}

void MapView::setCollectedClues(const QSet<QString> &collectedClues)
{
    m_collectedClues = collectedClues;
    update();
}

void MapView::setMovePath(const QVector<QPoint> &path, int consumedCount)
{
    m_movePath = path;
    m_consumedPathCount = qBound(0, consumedCount, path.size());
    update();
}

QSize MapView::minimumSizeHint() const
{
    return QSize(640, 480);
}

void MapView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.fillRect(rect(), QColor(3, 4, 7));

    if (!m_referenceMap.isNull()) {
        painter.setOpacity(0.06);
        painter.drawPixmap(rect(), m_referenceMap);
        painter.setOpacity(1.0);
    }

    if (!m_level || m_level->mapGrid.isEmpty()) {
        painter.setPen(QColor(230, 218, 180));
        painter.drawText(rect(), Qt::AlignCenter, "No map data loaded.");
        return;
    }

    const QRect bounds = mapBounds();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 110));
    painter.drawRect(bounds.adjusted(-16, -12, 16, 18));

    for (int row = 0; row < m_level->mapGrid.size(); ++row) {
        for (int column = 0; column < m_level->mapGrid.at(row).size(); ++column) {
            const QRect rect = tileRect(row, column);
            drawTile(painter, row, column, rect, m_level->mapGrid.at(row).at(column));
        }
    }

    for (int row = 0; row < m_level->mapGrid.size(); ++row) {
        for (int column = 0; column < m_level->mapGrid.at(row).size(); ++column) {
            const QRect rect = tileRect(row, column);
            drawObject(painter, rect, m_level->mapGrid.at(row).at(column));
        }
    }

    drawMovePath(painter);

    if (m_selectedTile.x() >= 0) {
        const QRect selected = tileRect(m_selectedTile.y(), m_selectedTile.x()).adjusted(2, 2, -2, -2);
        painter.setPen(QPen(QColor(74, 237, 255), 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(selected);
    }

    drawPlayer(painter);
}

void MapView::mousePressEvent(QMouseEvent *event)
{
    const QPoint tile = tileAtPosition(event->pos());
    if (tile.x() < 0) {
        return;
    }

    m_selectedTile = tile;
    update();
    emit tileClicked(tile.y(), tile.x());
}

QRect MapView::tileRect(int row, int column) const
{
    return tileOuterRect(row, column).adjusted(tilePadding, tilePadding, -tilePadding, -tilePadding);
}

QRect MapView::visualTileRect(const QPointF &position) const
{
    if (!m_level || m_level->mapWidth <= 0 || m_level->mapHeight <= 0) {
        return {};
    }

    const int tileSize = currentTileSize();
    const QRect bounds = mapBounds();
    const QPoint center(bounds.left() + qRound((position.x() + 0.5) * tileSize),
                        bounds.top() + qRound((position.y() + 0.5) * tileSize));
    return QRect(center.x() - tileSize / 2 + tilePadding,
                 center.y() - tileSize / 2 + tilePadding,
                 tileSize - tilePadding * 2,
                 tileSize - tilePadding * 2);
}

QRect MapView::tileOuterRect(int row, int column) const
{
    if (!m_level || m_level->mapWidth <= 0 || m_level->mapHeight <= 0) {
        return {};
    }

    const int tileSize = currentTileSize();
    const QRect bounds = mapBounds();
    return QRect(bounds.left() + column * tileSize, bounds.top() + row * tileSize, tileSize, tileSize);
}

QRect MapView::mapBounds() const
{
    if (!m_level || m_level->mapWidth <= 0 || m_level->mapHeight <= 0) {
        return {};
    }

    const int tileSize = currentTileSize();
    const int mapWidth = tileSize * m_level->mapWidth;
    const int mapHeight = tileSize * m_level->mapHeight;
    return QRect((width() - mapWidth) / 2, (height() - mapHeight) / 2, mapWidth, mapHeight);
}

int MapView::currentTileSize() const
{
    if (!m_level || m_level->mapWidth <= 0 || m_level->mapHeight <= 0) {
        return 64;
    }

    const int availableWidth = qMax(1, width() - mapMargin * 2);
    const int availableHeight = qMax(1, height() - mapMargin * 2);
    return qBound(56, qMin(availableWidth / m_level->mapWidth, availableHeight / m_level->mapHeight), 138);
}

QPoint MapView::tileAtPosition(const QPoint &position) const
{
    if (!m_level) {
        return QPoint(-1, -1);
    }

    for (int row = 0; row < m_level->mapGrid.size(); ++row) {
        for (int column = 0; column < m_level->mapGrid.at(row).size(); ++column) {
            if (tileOuterRect(row, column).contains(position)) {
                return QPoint(column, row);
            }
        }
    }
    return QPoint(-1, -1);
}

QString MapView::monsterSpritePath(const QString &monsterId) const
{
    static const QStringList sprites = {
        ":/images/assets/alice.png",
        ":/images/assets/cheshire_cat.png",
        ":/images/assets/dodo.png",
        ":/images/assets/gerda.png",
        ":/images/assets/jabberwock.png",
        ":/images/assets/mabel.png",
        ":/images/assets/node.png",
        ":/images/assets/red_hood.png"
    };
    return sprites.at(qHash(monsterId) % sprites.size());
}

bool MapView::isOpenTile(int row, int column) const
{
    if (!m_level || row < 0 || row >= m_level->mapGrid.size() || column < 0 || column >= m_level->mapGrid.at(row).size()) {
        return false;
    }
    return m_level->mapGrid.at(row).at(column) != "#";
}

bool MapView::wallTouchesOpenTile(int row, int column) const
{
    return isOpenTile(row - 1, column)
           || isOpenTile(row + 1, column)
           || isOpenTile(row, column - 1)
           || isOpenTile(row, column + 1)
           || isOpenTile(row - 1, column - 1)
           || isOpenTile(row - 1, column + 1)
           || isOpenTile(row + 1, column - 1)
           || isOpenTile(row + 1, column + 1);
}

void MapView::drawTile(QPainter &painter, int row, int column, const QRect &rect, const QString &tileId) const
{
    const bool wall = tileId == "#";
    if (wall && !wallTouchesOpenTile(row, column)) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 235));
        painter.drawRect(rect.adjusted(-tilePadding, -tilePadding, tilePadding, tilePadding));
        return;
    }

    painter.setPen(Qt::NoPen);
    const QColor base = wall ? wallColor(row, column) : floorColor(row, column);
    painter.setBrush(QColor(8, 8, 8));
    painter.drawRect(rect);

    if (wall) {
        if (!m_wallPixmap.isNull()) {
            painter.setOpacity(0.45);
            painter.drawPixmap(rect, m_wallPixmap);
            painter.setOpacity(1.0);
        }
        const int rows = 3;
        const int brickHeight = qMax(8, rect.height() / rows);
        for (int y = 0; y < rows; ++y) {
            const int offset = (y % 2) ? rect.width() / 6 : 0;
            const int brickWidth = qMax(16, rect.width() / 3);
            for (int x = -1; x < 4; ++x) {
                QRect brick(rect.left() + x * brickWidth + offset + 1,
                            rect.top() + y * brickHeight + 1,
                            brickWidth - 2,
                            brickHeight - 2);
                brick = brick.intersected(rect.adjusted(1, 1, -1, -1));
                if (brick.isEmpty()) {
                    continue;
                }
                const int shift = stableNoise(row + y, column + x, 11) % 30 - 12;
                painter.fillRect(brick, shiftedColor(base, shift));
                painter.fillRect(brick.adjusted(2, 2, -2, -brick.height() / 2), QColor(255, 255, 220, 18));
                painter.fillRect(QRect(brick.left(), brick.bottom() - 2, brick.width(), 2), QColor(0, 0, 0, 65));
            }
        }
        painter.fillRect(rect.adjusted(0, rect.height() * 2 / 3, 0, 0), QColor(0, 0, 0, 72));
        painter.setPen(QPen(QColor(184, 178, 148, 45), 1));
        painter.drawLine(rect.left() + 4, rect.top() + 3, rect.right() - 4, rect.top() + 3);
        return;
    }

    const int rows = 3;
    const int cols = 3;
    const int cellWidth = qMax(8, rect.width() / cols);
    const int cellHeight = qMax(8, rect.height() / rows);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const int inset = 1 + stableNoise(row + y, column + x, 17) % 2;
            QRect stone(rect.left() + x * cellWidth + inset,
                        rect.top() + y * cellHeight + inset,
                        cellWidth - inset * 2,
                        cellHeight - inset * 2);
            if (x == cols - 1) {
                stone.setRight(rect.right() - inset);
            }
            if (y == rows - 1) {
                stone.setBottom(rect.bottom() - inset);
            }
            const int shift = stableNoise(row + y, column + x, 23) % 28 - 10;
            painter.fillRect(stone, shiftedColor(base, shift));
            painter.fillRect(stone.adjusted(1, 1, -1, -stone.height() / 2), QColor(255, 255, 210, 16));
            painter.fillRect(QRect(stone.left(), stone.bottom() - 1, stone.width(), 1), QColor(0, 0, 0, 55));
        }
    }

    painter.setPen(QPen(QColor(10, 10, 10, 82), 1));
    const int crackOffset = stableNoise(row, column, 37) % qMax(8, rect.width() / 2);
    painter.drawLine(rect.left() + 7 + crackOffset / 2,
                     rect.top() + rect.height() / 4,
                     rect.left() + 10 + crackOffset,
                     rect.top() + rect.height() / 2);
    if ((row + column) % 2 == 0) {
        painter.drawLine(rect.center().x(), rect.center().y(), rect.center().x() + rect.width() / 5, rect.center().y() + 3);
    }

    if (!isOpenTile(row - 1, column)) {
        painter.fillRect(rect.left(), rect.top(), rect.width(), qMax(3, rect.height() / 12), QColor(0, 0, 0, 72));
    }
    if (!isOpenTile(row, column - 1)) {
        painter.fillRect(rect.left(), rect.top(), qMax(3, rect.width() / 12), rect.height(), QColor(0, 0, 0, 52));
    }
    if (!isOpenTile(row + 1, column)) {
        painter.fillRect(rect.left(), rect.bottom() - qMax(3, rect.height() / 10), rect.width(), qMax(3, rect.height() / 10), QColor(0, 0, 0, 92));
    }
    if (!isOpenTile(row, column + 1)) {
        painter.fillRect(rect.right() - qMax(3, rect.width() / 12), rect.top(), qMax(3, rect.width() / 12), rect.height(), QColor(0, 0, 0, 58));
    }
}

void MapView::drawMovePath(QPainter &painter) const
{
    if (m_movePath.isEmpty() || m_consumedPathCount >= m_movePath.size()) {
        return;
    }

    QVector<QPoint> points;
    points.append(m_playerPosition);
    for (int i = m_consumedPathCount; i < m_movePath.size(); ++i) {
        points.append(m_movePath.at(i));
    }
    if (points.size() < 2) {
        return;
    }

    QPen glow(QColor(0, 0, 0, 120), 10, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
    glow.setDashPattern({1.2, 1.8});
    QPen core(QColor(3, 4, 7, 230), 4, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
    core.setDashPattern({1.0, 2.2});

    auto tileCenter = [this](const QPoint &tile) {
        return tileRect(tile.y(), tile.x()).center();
    };

    painter.setRenderHint(QPainter::Antialiasing, true);
    for (int pass = 0; pass < 2; ++pass) {
        painter.setPen(pass == 0 ? glow : core);
        for (int i = 1; i < points.size(); ++i) {
            painter.drawLine(tileCenter(points.at(i - 1)), tileCenter(points.at(i)));
        }
    }
    painter.setRenderHint(QPainter::Antialiasing, false);
}

void MapView::drawObject(QPainter &painter, const QRect &rect, const QString &tileId) const
{
    if (tileId == "#" || tileId == "." || tileId == "start") {
        return;
    }

    if (tileId.startsWith("chest")) {
        if (!m_openedChests.contains(tileId)) {
            drawChest(painter, rect, tileId);
        }
        return;
    }

    if (tileId.startsWith("clue")) {
        if (m_collectedClues.contains(tileId)) {
            return;
        }
        drawClue(painter, rect);
        return;
    }

    if (tileId == "boss") {
        if (m_defeatedMonsters.contains("boss")) {
            return;
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 120));
        painter.drawEllipse(QRect(rect.left() + rect.width() / 7, rect.bottom() - rect.height() / 5,
                                  rect.width() * 5 / 7, rect.height() / 5));
        const QRect spriteRect = spriteRectInTile(rect, m_bossPixmap, 82, 96, -2);
        painter.drawPixmap(spriteRect, m_bossPixmap);
        return;
    }

    if (tileId.startsWith("monster")) {
        if (m_defeatedMonsters.contains(tileId)) {
            return;
        }
        const QPixmap sprite = loadSprite(monsterSpritePath(tileId));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 105));
        painter.drawEllipse(QRect(rect.left() + rect.width() / 4, rect.bottom() - rect.height() / 6,
                                  rect.width() / 2, rect.height() / 7));
        const QRect spriteRect = spriteRectInTile(rect, sprite, 62, 78, -2);
        painter.drawPixmap(spriteRect, sprite);
    }
}

void MapView::drawChest(QPainter &painter, const QRect &rect, const QString &tileId) const
{
    bool forced = false;
    if (m_level && m_level->chests.contains(tileId)) {
        forced = m_level->chests.value(tileId).forcedPick;
    }
    const QPixmap &chestPixmap = forced ? m_forcedChestPixmap : m_unforcedChestPixmap;
    if (!chestPixmap.isNull()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 105));
        painter.drawEllipse(QRect(rect.left() + rect.width() / 7, rect.bottom() - rect.height() / 5,
                                  rect.width() * 5 / 7, rect.height() / 5));
        const QRect spriteRect = spriteRectInTile(rect, chestPixmap, forced ? 100 : 112, forced ? 96 : 92, 0);
        painter.drawPixmap(spriteRect, chestPixmap);
        return;
    }

    QRect chest = rect.adjusted(rect.width() / 6, rect.height() / 3, -rect.width() / 6, -rect.height() / 8);
    painter.setPen(QPen(QColor(245, 197, 70), 2));
    painter.setBrush(QColor(82, 45, 18));
    painter.drawRoundedRect(chest, 4, 4);
    painter.setBrush(QColor(139, 87, 29));
    painter.drawRect(chest.adjusted(0, -chest.height() / 3, 0, -chest.height() / 2));
    painter.setBrush(QColor(255, 221, 87));
    painter.drawEllipse(chest.center(), qMax(3, chest.width() / 10), qMax(3, chest.width() / 10));
}

void MapView::drawClue(QPainter &painter, const QRect &rect) const
{
    if (!m_cluePixmap.isNull()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 100));
        painter.drawEllipse(QRect(rect.left() + rect.width() / 6, rect.bottom() - rect.height() / 5,
                                  rect.width() * 2 / 3, rect.height() / 5));
        const QRect spriteRect = spriteRectInTile(rect, m_cluePixmap, 94, 82, 0);
        painter.drawPixmap(spriteRect, m_cluePixmap);
        return;
    }

    QRect scroll = rect.adjusted(rect.width() / 5, rect.height() / 3, -rect.width() / 5, -rect.height() / 5);
    painter.setPen(QPen(QColor(235, 216, 164), 2));
    painter.setBrush(QColor(186, 154, 96));
    painter.drawRoundedRect(scroll, 7, 7);
    painter.setPen(QPen(QColor(72, 52, 33), 2));
    painter.drawLine(scroll.left() + 8, scroll.center().y(), scroll.right() - 8, scroll.center().y());
    painter.setPen(QPen(QColor(72, 210, 226), 3));
    painter.drawEllipse(scroll.right() - scroll.width() / 4, scroll.top() + 4, 10, 10);
}

void MapView::drawPlayer(QPainter &painter) const
{
    const QRect playerTile = visualTileRect(m_playerVisualPosition);
    if (playerTile.isEmpty()) {
        return;
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 125));
    painter.drawEllipse(QRect(playerTile.left() + playerTile.width() / 5,
                              playerTile.bottom() - playerTile.height() / 5,
                              playerTile.width() * 3 / 5,
                              playerTile.height() / 5));

    const QPixmap frame = currentPlayerFrame();
    if (!frame.isNull()) {
        const QRect playerRect = spriteRectInTile(playerTile, frame, 72, 92, -2);
        painter.drawPixmap(playerRect, frame);
        return;
    }

    if (!m_playerPixmap.isNull()) {
        const QRect playerRect = spriteRectInTile(playerTile, m_playerPixmap, 72, 92, -2);
        painter.drawPixmap(playerRect, m_playerPixmap);
    } else {
        painter.setBrush(QColor(255, 211, 91));
        painter.setPen(QPen(QColor(27, 17, 7), 2));
        painter.drawEllipse(playerTile.adjusted(8, 4, -8, -4));
    }
}

QPixmap MapView::currentPlayerFrame() const
{
    if (m_playerFrames.size() < 8) {
        return {};
    }

    int row = 0;
    if (m_playerWalking) {
        // Sheet rows: idle down/up/left/right, walk down/up/right/left.
        if (m_playerFacing == 0) {
            row = 4;
        } else if (m_playerFacing == 1) {
            row = 5;
        } else if (m_playerFacing == 2) {
            row = 7;
        } else {
            row = 6;
        }
    } else {
        row = qBound(0, m_playerFacing, 3);
    }

    if (row < 0 || row >= m_playerFrames.size() || m_playerFrames.at(row).isEmpty()) {
        return {};
    }
    const int frame = m_playerWalking ? m_playerFrameIndex % m_playerFrames.at(row).size() : 0;
    return m_playerFrames.at(row).at(frame);
}
