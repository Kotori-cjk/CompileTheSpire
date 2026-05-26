#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "challengerepository.h"

#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void applyDefaultSettings();
    void applyVisualStyle();
    void refreshGameUi();
    void resetRun();
    void loadChallenges();
    void refreshMapUi();
    void startChallenge(int challengeIndex);
    void submitAnswer();

    Ui::MainWindow *ui;
    ChallengeRepository challengeRepository;
    int gold = 99;
    int floor = 1;
    int currentChallengeIndex = 0;
    bool challengeSolved = false;
};
#endif // MAINWINDOW_H
