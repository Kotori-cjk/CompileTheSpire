#ifndef SAVEMANAGER_H
#define SAVEMANAGER_H
#include<QString>
#include<QVector>

class SaveManager
{
public:
    SaveManager();
    static const QString SAVE_PATH;
    void Init(int totalLevels);
    //游戏主进程拿到关卡后调用该初始化函数，防止数据为空
    bool Save()const;
    bool Load(int currentLevelCount);
    bool isUnlocked(int levelIndex)const;
    void Unlock(int levelIndex);
    void UnlockAll();
    int levelCount();
    void deleteSave();
    void hardReset();

private:
    int totalLevelCount;
    QVector<int>unlockState;
};

#endif // SAVEMANAGER_H

//Json结构:两个int字面意思，一个bool数组存解锁状态
//成员函数均为字面意思
