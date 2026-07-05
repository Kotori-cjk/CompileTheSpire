#ifndef STAGECATALOG_H
#define STAGECATALOG_H

#include "leveldata.h"

#include <QString>
#include <QStringList>
#include <QVector>

struct StageCard
{
    int levelIndex;
    bool isEx;
    QString title;
    QString subtitle;
    QString hint;
};

inline constexpr int stagesPerPage = 3;

QVector<StageCard> stageCatalog();
QStringList fallbackLevelPaths();
LevelData createPreviewLevel();

#endif // STAGECATALOG_H
