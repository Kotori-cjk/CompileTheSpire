#ifndef LEVELDATA_H
#define LEVELDATA_H
#include<QVector>
#include<QString>
#include<QMap>
#include<QJsonDocument>
#include<QJsonObject>
#include<QJsonArray>
#include<QDir>
#include<QPoint>

struct Space{
    QString spaceId;
    QString type;
    QStringList values;
};

struct Monster{
    QPoint pos;
    QString monsterId;
    QString pic;
    QString nickname;
    QString name;
    QString type;
    QString codeTemplate;
    QVector<Space>spaces;
    QStringList referencedClues;
};

struct Boss:public Monster{
    QString input,output;
};

struct CodeBlock {
    QString blockId;
    QString code;
};

struct Chest {
    QPoint pos;
    QString chestId;
    bool forcedPick;
    bool repeat;
    QMap<QString,CodeBlock>blocks;
};

struct Clue{
    QPoint pos;
    QString val;
};

struct SynthesisCell{
    QString type;//"clue" "space"
    QString id;//"clue_1" "space_1"
};

struct Synthesis{
    QStringList text;
    QVector<SynthesisCell>cell;
    int cluecnt,spacecnt;
};

Synthesis templateBreakdown(QString codeTemplate);//工具函数 拆分一个转义模板 可能会有用

class LevelData
{
private:
    static QVector<Space> parseSpaces(const QJsonObject &spacesObj);
    static QMap<QString,Chest> parseChests(const QJsonObject &chestsObj);
    static QMap<QString,Monster> parseMonsters(const QJsonObject &monstersObj);
    //工具函数

public:
    LevelData();
    int levelIndex;
    int mapWidth,mapHeight;
    QStringList specialTags;
    int bagSize;
    QVector<QVector<QString>>mapGrid;
    QMap<QString,Chest>chests;
    QMap<QString,Clue>clues;
    QMap<QString,Monster>monsters;
    Boss boss;
    QString endText;
    QPoint startpos;
    QString levelType;
    bool LoadFromJson(const QString& filePath,QString* errorMessage=nullptr);
    //从Json中读入，filePath为读取路径，errorMessage为可选的传出报错信息的指针
};

QVector<LevelData> LoadDirectory(const QString& dirPath,QStringList* errors=nullptr);
//参数1:string形式的文件夹路径 参数2:errorMsg指针
//从文件夹中读入若干个json,输出一个关卡列表

#endif // LEVELDATA_H
