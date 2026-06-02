#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mapview.h"
#include "codeblockui.h"
#include "stagecatalog.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDrag>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSlider>
#include <QStyle>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

void MainWindow::showBagDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Bag");
    dialog.setModal(true);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *title = new QLabel("Code Bag", &dialog);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);

    QFrame *board = new QFrame(&dialog);
    board->setObjectName("bagDialogBoard");
    board->setMinimumSize(900, 560);
    board->setStyleSheet("QFrame#bagDialogBoard { border-image: url(:/images/assets/bag_background.png) 0 0 0 0 stretch stretch; }");
    QGridLayout *grid = new QGridLayout(board);
    grid->setContentsMargins(130, 110, 130, 110);
    grid->setHorizontalSpacing(28);
    grid->setVerticalSpacing(24);

    QVector<QPair<QString, QString>> codeItems;
    for (const QString &blockId : bagBlocks) {
        codeItems.append(qMakePair(blockId, codeForBlock(blockId)));
    }

    constexpr int iconSlots = 10;
    for (int i = 0; i < iconSlots; ++i) {
        QToolButton *icon = new QToolButton(board);
        icon->setFixedSize(86, 86);
        icon->setToolButtonStyle(Qt::ToolButtonIconOnly);
        icon->setIcon(QIcon(i < codeItems.size() ? codeBlockIconPath(codeItems.at(i).first)
                                                  : ":/images/assets/natural.png"));
        icon->setIconSize(QSize(70, 70));
        icon->setStyleSheet("QToolButton { background: rgba(31, 23, 16, 210); border: 2px solid #d7b06a; border-radius: 6px; color: #ffe8ad; font-weight: 700; }"
                            "QToolButton:hover { background: rgba(55, 40, 24, 235); border-color: #49e6ff; color: #ffffff; }");
        if (i < codeItems.size()) {
            const QString blockId = codeItems.at(i).first;
            const QString code = codeItems.at(i).second;
            icon->setToolTip(formatCodeBlockForDisplay(code));
            installHoverPopup(icon, codeBlockPopupHtml(code, typeForBlock(blockId)));
            connect(icon, &QToolButton::clicked, &dialog, [this, &dialog, blockId, code]() {
                QDialog detail(&dialog);
                detail.setWindowTitle("Code Block");
                QVBoxLayout *detailLayout = new QVBoxLayout(&detail);
                QTextBrowser *codeView = new QTextBrowser(&detail);
                codeView->setReadOnly(true);
                codeView->setContextMenuPolicy(Qt::NoContextMenu);
                codeView->setTextInteractionFlags(Qt::NoTextInteraction);
                codeView->setHtml(QString("<pre style=\"font-family:Consolas; font-size:15px; color:#f9f1d0; background:#111827; padding:14px;\">%1</pre>")
                                      .arg(codeBlockHtml(code)));
                QDialogButtonBox *close = new QDialogButtonBox(QDialogButtonBox::Close, &detail);
                QPushButton *discardButton = close->addButton("Discard", QDialogButtonBox::DestructiveRole);
                QObject::connect(close, &QDialogButtonBox::rejected, &detail, &QDialog::reject);
                QObject::connect(discardButton, &QPushButton::clicked, &detail, [this, &detail, blockId]() {
                    if (!gameEngine.m_bag || !gameEngine.m_bag->bagRemove(blockId)) {
                        QMessageBox::warning(&detail, "Code Block", "Unable to discard this code block.");
                        return;
                    }
                    syncFromEngineState();
                    detail.accept();
                });
                detailLayout->addWidget(codeView);
                detailLayout->addWidget(close);
                detail.resize(520, 360);
                if (detail.exec() == QDialog::Accepted) {
                    dialog.accept();
                }
            });
        } else {
            icon->setIcon(QIcon());
            icon->setText("-");
            icon->setEnabled(false);
        }
        grid->addWidget(icon, i / 5, i % 5);
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(title);
    layout->addWidget(board);
    layout->addWidget(buttons);
    dialog.resize(980, 720);
    dialog.exec();
}

void MainWindow::showManualDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Monster Manual");
    dialog.setModal(true);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto spriteForMonster = [](const QString &monsterId, const Monster &monster) {
        if (monsterId == "boss" || monster.monsterId == "boss") {
            return QString(":/images/assets/marry_ann.png");
        }
        const QString normalizedPic = monster.pic.mid(monster.pic.lastIndexOf(QRegularExpression("[/\\\\]")) + 1);
        if (!normalizedPic.isEmpty()) {
            const QString resourcePath = ":/images/assets/" + normalizedPic;
            if (!QPixmap(resourcePath).isNull()) {
                return resourcePath;
            }
        }
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
    };

    auto monsterDisplayName = [](const QString &monsterId, const Monster &monster) {
        return monster.nickname.isEmpty() ? monsterId : monster.nickname;
    };

    auto showMonsterDetail = [this, &dialog, &spriteForMonster, &monsterDisplayName](const QString &monsterId, const Monster &monster) {
        QDialog detail(&dialog);
        detail.setWindowTitle(monsterDisplayName(monsterId, monster));
        detail.setModal(true);
        QVBoxLayout *detailRoot = new QVBoxLayout(&detail);
        detailRoot->setContentsMargins(0, 0, 0, 0);

        QFrame *board = new QFrame(&detail);
        board->setObjectName("manualDetailBoard");
        board->setMinimumSize(980, 620);
        board->setStyleSheet(
            "QFrame#manualDetailBoard { border-image: url(:/images/assets/manul_background.png) 0 0 0 0 stretch stretch; }"
            "QLabel { color: #f6e7bd; }"
        );
        QHBoxLayout *boardLayout = new QHBoxLayout(board);
        boardLayout->setContentsMargins(70, 72, 70, 70);
        boardLayout->setSpacing(26);

        QFrame *portraitPanel = new QFrame(board);
        portraitPanel->setStyleSheet("QFrame { background: rgba(13, 16, 20, 170); border: 2px solid rgba(215, 176, 106, 190); border-radius: 8px; }");
        QVBoxLayout *portraitLayout = new QVBoxLayout(portraitPanel);
        portraitLayout->setContentsMargins(18, 18, 18, 18);
        QLabel *sprite = new QLabel(portraitPanel);
        sprite->setAlignment(Qt::AlignCenter);
        sprite->setMinimumSize(260, 310);
        const QPixmap pix(spriteForMonster(monsterId, monster));
        if (!pix.isNull()) {
            sprite->setPixmap(pix.scaled(250, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        portraitLayout->addWidget(sprite, 1);
        boardLayout->addWidget(portraitPanel, 0);

        QFrame *infoPanel = new QFrame(board);
        infoPanel->setStyleSheet("QFrame { background: rgba(8, 10, 14, 178); border: 2px solid rgba(79, 234, 255, 145); border-radius: 8px; }");
        QVBoxLayout *infoLayout = new QVBoxLayout(infoPanel);
        infoLayout->setContentsMargins(22, 18, 22, 18);
        infoLayout->setSpacing(10);

        QLabel *name = new QLabel(monsterDisplayName(monsterId, monster), infoPanel);
        name->setStyleSheet("QLabel { color: #ffd86b; font-size: 28px; font-weight: 900; }");
        name->setWordWrap(true);
        infoLayout->addWidget(name);

        QTextBrowser *codeView = new QTextBrowser(infoPanel);
        codeView->setHtml(renderMonsterCodeHtml(monster));
        codeView->setContextMenuPolicy(Qt::NoContextMenu);
        codeView->setTextInteractionFlags(Qt::NoTextInteraction);
        codeView->setStyleSheet(
            "QTextBrowser { background: rgba(7, 12, 20, 218); border: 1px solid rgba(215, 176, 106, 135);"
            "border-radius: 6px; padding: 4px; color: #f9f1d0; }"
        );
        codeView->setMinimumHeight(285);
        infoLayout->addWidget(codeView, 1);

        if (monster.spaces.isEmpty()) {
            QLabel *friendly = new QLabel("Friendly", infoPanel);
            friendly->setAlignment(Qt::AlignCenter);
            friendly->setStyleSheet("QLabel { color: #b9ffd0; font-size: 16px; font-weight: 900; background: rgba(28, 109, 50, 160); border: 1px solid rgba(139, 232, 154, 210); border-radius: 6px; padding: 7px; }");
            infoLayout->addWidget(friendly);
        }

        if (monsterId == "boss") {
            const Boss boss = levels.at(currentLevelIndex).boss;
            QLabel *io = new QLabel(QString("Expected input: %1\nExpected output: %2")
                                        .arg(boss.input.isEmpty() ? "None" : boss.input,
                                             boss.output.isEmpty() ? "None" : boss.output),
                                    infoPanel);
            io->setStyleSheet("QLabel { color: #a8f4bf; font-family: 'Consolas', monospace; font-size: 14px; }");
            io->setWordWrap(true);
            infoLayout->addWidget(io);
        }

        boardLayout->addWidget(infoPanel, 1);
        detailRoot->addWidget(board);
        detail.resize(1080, 700);
        detail.exec();
    };

    QLabel *title = new QLabel("Monster Manual", &dialog);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        "QLabel { color: #ffd86b; font-size: 30px; font-weight: 900;"
        "background: rgba(18, 21, 28, 210); border: 2px solid #d7b06a; border-radius: 8px; padding: 8px; }"
    );

    QFrame *board = new QFrame(&dialog);
    board->setObjectName("manualBoard");
    board->setMinimumSize(900, 560);
    board->setStyleSheet("QFrame#manualBoard { border-image: url(:/images/assets/manul_background.png) 0 0 0 0 stretch stretch; }");
    QVBoxLayout *boardLayout = new QVBoxLayout(board);
    boardLayout->setContentsMargins(80, 78, 80, 70);
    boardLayout->setSpacing(14);

    QLabel *pageLabel = new QLabel(board);
    pageLabel->setAlignment(Qt::AlignCenter);
    pageLabel->setStyleSheet("QLabel { color: #4feaff; font-size: 18px; font-weight: 800; background: rgba(9, 12, 18, 170); border: 1px solid rgba(79, 234, 255, 135); border-radius: 6px; padding: 6px; }");
    boardLayout->addWidget(pageLabel);

    QWidget *gridHolder = new QWidget(board);
    QGridLayout *grid = new QGridLayout(gridHolder);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setHorizontalSpacing(22);
    grid->setVerticalSpacing(18);
    boardLayout->addWidget(gridHolder, 1);

    QHBoxLayout *pager = new QHBoxLayout();
    pager->setSpacing(18);
    QPushButton *prevButton = new QPushButton("<", board);
    QPushButton *nextButton = new QPushButton(">", board);
    const QString pagerButtonStyle =
        "QPushButton { min-width: 86px; min-height: 40px; color: #4feaff; font-size: 22px; font-weight: 900;"
        "background: rgba(7, 17, 25, 210); border: 2px solid #24dff5; border-radius: 8px; }"
        "QPushButton:hover { background: rgba(35, 102, 117, 225); }"
        "QPushButton:disabled { color: #5f6570; border-color: #4a515d; background: rgba(15, 18, 24, 160); }";
    prevButton->setStyleSheet(pagerButtonStyle);
    nextButton->setStyleSheet(pagerButtonStyle);
    pager->addStretch();
    pager->addWidget(prevButton);
    pager->addWidget(nextButton);
    pager->addStretch();
    boardLayout->addLayout(pager);

    QVector<QPair<QString, Monster>> monsterItems;
    if (currentLevelIndex >= 0 && currentLevelIndex < levels.size()) {
        const LevelData &level = levels.at(currentLevelIndex);
        QSet<QString> listedMonsterIds;
        for (auto it = level.monsters.cbegin(); it != level.monsters.cend(); ++it) {
            const QString monsterId = (it.key() == "boss" || it.value().monsterId == "boss") ? QString("boss") : it.key();
            if (listedMonsterIds.contains(monsterId)) {
                continue;
            }
            listedMonsterIds.insert(monsterId);
            monsterItems.append(qMakePair(monsterId, it.value()));
        }
        if (!level.boss.monsterId.isEmpty() && !listedMonsterIds.contains("boss")) {
            monsterItems.append(qMakePair(QString("boss"), static_cast<Monster>(level.boss)));
        }
    }

    const int itemsPerPage = 4;
    const int totalPages = qMax(1, (monsterItems.size() + itemsPerPage - 1) / itemsPerPage);
    int currentPage = 0;

    std::function<void()> refreshPage = [&]() {
        clearLayout(grid);
        pageLabel->setText(QString("PAGE %1 / %2").arg(currentPage + 1).arg(totalPages));
        prevButton->setEnabled(currentPage > 0);
        nextButton->setEnabled(currentPage + 1 < totalPages);

        const int start = currentPage * itemsPerPage;
        for (int i = 0; i < itemsPerPage; ++i) {
            const int itemIndex = start + i;
            QToolButton *card = new QToolButton(gridHolder);
            card->setFixedSize(330, 150);
            card->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            card->setIconSize(QSize(92, 92));
            card->setStyleSheet(
                "QToolButton { color: #f6e7bd; font-size: 16px; font-weight: 800; text-align: left;"
                "background: rgba(9, 12, 18, 190); border: 2px solid rgba(215, 176, 106, 190); border-radius: 8px; padding: 12px; }"
                "QToolButton:hover { border-color: #4feaff; background: rgba(15, 35, 45, 220); }"
            );
            if (itemIndex < monsterItems.size()) {
                const QString monsterId = monsterItems.at(itemIndex).first;
                const Monster monster = monsterItems.at(itemIndex).second;
                const bool friendly = monster.spaces.isEmpty();
                const bool defeated = !friendly && defeatedMonsters.contains(monsterId);
                card->setText(QString("%1%2").arg(monsterDisplayName(monsterId, monster),
                                                  monsterId == "boss" ? "\nBOSS" : QString()));
                card->setIcon(QIcon(spriteForMonster(monsterId, monster)));
                if (friendly) {
                    card->setStyleSheet(
                        "QToolButton { color: #eefbea; font-size: 16px; font-weight: 800; text-align: left;"
                        "background: rgba(34, 92, 47, 185); border: 2px solid rgba(139, 232, 154, 210); border-radius: 8px; padding: 12px; }"
                        "QToolButton:hover { border-color: #4feaff; background: rgba(42, 116, 60, 220); }"
                    );
                } else if (defeated) {
                    card->setStyleSheet(
                        "QToolButton { color: #eefbea; font-size: 16px; font-weight: 800; text-align: left;"
                        "background: rgba(34, 92, 47, 185); border: 2px solid rgba(139, 232, 154, 210); border-radius: 8px; padding: 12px; }"
                        "QToolButton:hover { border-color: #4feaff; background: rgba(42, 116, 60, 220); }"
                    );
                }
                installHoverPopup(card,
                                  QString("<b>%1</b><br>%2%3")
                                      .arg(monsterDisplayName(monsterId, monster).toHtmlEscaped(),
                                           monsterId == "boss" ? "Boss<br>" : QString(),
                                           friendly ? "Friendly" : (defeated ? "Defeated" : "Not defeated")));
                connect(card, &QToolButton::clicked, &dialog, [&, monsterId, monster]() {
                    showMonsterDetail(monsterId, monster);
                });
            } else {
                card->setText("EMPTY SLOT");
                card->setEnabled(false);
            }
            grid->addWidget(card, i / 2, i % 2);
        }
    };

    connect(prevButton, &QPushButton::clicked, &dialog, [&]() {
        if (currentPage > 0) {
            --currentPage;
            refreshPage();
        }
    });
    connect(nextButton, &QPushButton::clicked, &dialog, [&]() {
        if (currentPage + 1 < totalPages) {
            ++currentPage;
            refreshPage();
        }
    });
    refreshPage();

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(title);
    layout->addWidget(board);
    layout->addWidget(buttons);
    dialog.resize(1040, 760);
    dialog.exec();
}

void MainWindow::showVictorySettlement()
{
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    playBgm("assets/audio/bgm_victory.mp3");

    QDialog dialog(this);
    dialog.setWindowTitle("Victory Settlement");
    dialog.setModal(true);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(0, 0, 0, 0);

    QFrame *board = new QFrame(&dialog);
    board->setObjectName("victoryBoard");
    board->setMinimumSize(980, 620);
    board->setStyleSheet(
        "QFrame#victoryBoard { border-image: url(:/images/assets/victory_settlement.png) 0 0 0 0 stretch stretch; }"
        "QLabel { color: #f7e7bd; }"
    );

    QVBoxLayout *layout = new QVBoxLayout(board);
    layout->setContentsMargins(110, 210, 110, 50);
    layout->setSpacing(14);

    QFrame *summaryPanel = new QFrame(board);
    summaryPanel->setObjectName("victorySummaryPanel");
    summaryPanel->setStyleSheet(
        "QFrame#victorySummaryPanel { background: rgba(7, 8, 12, 92);"
        "border: 1px solid rgba(226, 181, 86, 110); border-radius: 8px; }"
    );
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(24, 14, 24, 14);
    summaryLayout->setSpacing(10);

    const LevelData &level = levels.at(currentLevelIndex);
    QLabel *stageLine = new QLabel(QString("Stage %1 Cleared").arg(currentLevelIndex + 1), summaryPanel);
    stageLine->setAlignment(Qt::AlignCenter);
    stageLine->setStyleSheet("QLabel { color: #4feaff; font-size: 26px; font-weight: 900; background: transparent; }");
    summaryLayout->addWidget(stageLine);

    QLabel *message = new QLabel(level.endText.isEmpty() ? "The spire compiled successfully." : level.endText, summaryPanel);
    message->setAlignment(Qt::AlignCenter);
    message->setWordWrap(true);
    message->setStyleSheet("QLabel { color: #fff1c2; font-size: 18px; font-weight: 800; background: rgba(7, 8, 12, 70); border: 1px solid rgba(226, 181, 86, 80); border-radius: 6px; padding: 10px; }");
    summaryLayout->addWidget(message);

    QFrame *stats = new QFrame(summaryPanel);
    stats->setStyleSheet("QFrame { background: rgba(5, 7, 10, 92); border: 1px solid rgba(79, 234, 255, 95); border-radius: 6px; }");
    QGridLayout *statsLayout = new QGridLayout(stats);
    statsLayout->setContentsMargins(20, 14, 20, 14);
    statsLayout->setHorizontalSpacing(24);
    statsLayout->setVerticalSpacing(10);

    auto addStat = [statsLayout](int row, int column, const QString &label, const QString &value) {
        QLabel *labelWidget = new QLabel(label);
        labelWidget->setStyleSheet("QLabel { color: rgba(247, 231, 189, 190); font-size: 13px; font-weight: 700; }");
        QLabel *valueWidget = new QLabel(value);
        valueWidget->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueWidget->setStyleSheet("QLabel { color: #ffe082; font-size: 20px; font-weight: 900; }");
        statsLayout->addWidget(labelWidget, row, column * 2);
        statsLayout->addWidget(valueWidget, row, column * 2 + 1);
    };

    const QVector<int> unlockedThisClear = newlyUnlockedStageIndexes;
    addStat(0, 0, "Clues Revealed", QString("%1 / %2").arg(collectedClues.size()).arg(level.clues.size()));
    addStat(0, 1, "Unlocked", unlockedThisClear.isEmpty() ? "No New Stage" : "New Stage");
    summaryLayout->addWidget(stats);

    if (!unlockedThisClear.isEmpty()) {
        QFrame *unlockPanel = new QFrame(summaryPanel);
        unlockPanel->setStyleSheet(
            "QFrame { background: rgba(5, 14, 22, 145); border: 1px solid rgba(79, 234, 255, 130); border-radius: 6px; }"
            "QLabel { background: transparent; }"
        );
        QVBoxLayout *unlockLayout = new QVBoxLayout(unlockPanel);
        unlockLayout->setContentsMargins(18, 12, 18, 12);
        unlockLayout->setSpacing(10);

        QLabel *unlockTitle = new QLabel("New Stage Unlocked", unlockPanel);
        unlockTitle->setAlignment(Qt::AlignCenter);
        unlockTitle->setStyleSheet("QLabel { color: #4feaff; font-size: 16px; font-weight: 900; }");
        unlockLayout->addWidget(unlockTitle);

        QHBoxLayout *unlockRow = new QHBoxLayout();
        unlockRow->setSpacing(12);
        unlockRow->addStretch();
        const QVector<StageCard> stages = stageCatalog();
        for (int levelIndex : unlockedThisClear) {
            const auto stageIt = std::find_if(stages.cbegin(), stages.cend(), [levelIndex](const StageCard &stage) {
                return stage.levelIndex == levelIndex;
            });
            const QString stageId = stageIt != stages.cend()
                                        ? (stageIt->isEx ? QString("EX-%1").arg(levelIndex - 8) : QString::number(levelIndex + 1))
                                        : QString::number(levelIndex + 1);
            QFrame *badge = new QFrame(unlockPanel);
            badge->setFixedSize(92, 92);
            badge->setStyleSheet(
                "QFrame { background: qradialgradient(cx:0.5, cy:0.42, radius:0.72,"
                "stop:0 rgba(140, 255, 255, 245), stop:0.24 rgba(40, 183, 212, 238),"
                "stop:0.68 rgba(8, 45, 64, 245), stop:1 rgba(1, 8, 14, 255));"
                "border: 4px solid rgba(80, 240, 255, 225); border-radius: 46px; }"
            );
            QVBoxLayout *badgeLayout = new QVBoxLayout(badge);
            badgeLayout->setContentsMargins(6, 6, 6, 6);
            QLabel *badgeText = new QLabel(stageId, badge);
            badgeText->setAlignment(Qt::AlignCenter);
            badgeText->setStyleSheet("QLabel { color: #eaffff; font-family: Georgia, 'Times New Roman'; font-size: 24px; font-weight: 900; }");
            badgeLayout->addWidget(badgeText);
            unlockRow->addWidget(badge);
        }
        unlockRow->addStretch();
        unlockLayout->addLayout(unlockRow);
        summaryLayout->addWidget(unlockPanel);
    }

    layout->addWidget(summaryPanel, 0);
    layout->addStretch(1);

    QHBoxLayout *actions = new QHBoxLayout();
    actions->setSpacing(18);
    auto makeButton = [board](const QString &text) {
        QPushButton *button = new QPushButton(text, board);
        button->setMinimumSize(190, 54);
        button->setStyleSheet(
            "QPushButton { color: #fff1c2; font-size: 18px; font-weight: 900;"
            "background: rgba(9, 13, 18, 175); border: 2px solid #d7b06a; border-radius: 7px; }"
            "QPushButton:hover { color: white; border-color: #4feaff; background: rgba(18, 47, 58, 225); }"
            "QPushButton:pressed { background: rgba(6, 10, 15, 235); }"
        );
        return button;
    };
    QPushButton *stayButton = makeButton("Continue");
    QPushButton *stageButton = makeButton("Select Stage");
    QPushButton *menuButton = makeButton("Main Menu");
    actions->addStretch();
    actions->addWidget(stayButton);
    actions->addWidget(stageButton);
    actions->addWidget(menuButton);
    actions->addStretch();
    layout->addLayout(actions);

    connect(stayButton, &QPushButton::clicked, &dialog, [this, &dialog]() {
        const int nextLevelIndex = currentLevelIndex + 1;
        dialog.accept();
        if (nextLevelIndex >= 0 && nextLevelIndex < levels.size() && isLevelUnlocked(nextLevelIndex)) {
            QTimer::singleShot(0, this, [this, nextLevelIndex]() {
                startLevel(nextLevelIndex);
            });
        } else {
            QTimer::singleShot(0, this, [this]() {
                refreshLevelSelectUi();
                ui->stackedWidget->setCurrentWidget(ui->mapPage);
            });
        }
    });
    connect(stageButton, &QPushButton::clicked, &dialog, [this, &dialog]() {
        refreshLevelSelectUi();
        ui->stackedWidget->setCurrentWidget(ui->mapPage);
        dialog.accept();
    });
    connect(menuButton, &QPushButton::clicked, &dialog, [this, &dialog]() {
        ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
        mainMenuButtonBar->show();
        positionMainMenuButtons();
        dialog.accept();
    });

    root->addWidget(board);
    dialog.resize(1080, 700);
    dialog.exec();
    newlyUnlockedStageIndexes.clear();
}

void MainWindow::handleChest(const QString &chestId)
{
    const Chest chest = levels.at(currentLevelIndex).chests.value(chestId);
    if (!gameEngine.m_bag || !chestHasAvailableBlocks(chestId)) {
        ui->combatLogLabel->setText("This chest is already empty.");
        refreshGameUi();
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Chest");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *summary = new QLabel(QString("强制拾取: %1\n可多次拾取: %2")
                                     .arg(chest.forcedPick ? "是" : "否",
                                          chest.repeat ? "是" : "否"),
                                 &dialog);
    summary->setWordWrap(true);
    QWidget *contents = new QWidget(&dialog);
    QGridLayout *contentGrid = new QGridLayout(contents);
    contentGrid->setSpacing(12);
    QString selectedBlockId;
    int blockIndex = 0;
    for (const CodeBlock &block : gameEngine.m_bag->blocksRemaining(chestId)) {
        QToolButton *blockIcon = new QToolButton(contents);
        blockIcon->setFixedSize(128, 68);
        blockIcon->setToolButtonStyle(Qt::ToolButtonIconOnly);
        blockIcon->setIcon(QIcon(codeBlockIconPath(block.blockId)));
        blockIcon->setIconSize(QSize(58, 58));
        blockIcon->setToolTip(formatCodeBlockForDisplay(block.code));
        installHoverPopup(blockIcon, codeBlockPopupHtml(block.code, typeForBlock(block.blockId)));
        blockIcon->setStyleSheet("QToolButton { background: #15242b; border: 2px solid #d7b06a; border-radius: 5px; color: #ffe8ad; font-weight: 700; }"
                                 "QToolButton:hover { border-color: #49e6ff; color: white; }");
        connect(blockIcon, &QToolButton::clicked, &dialog, [&dialog, &selectedBlockId, block]() {
            selectedBlockId = block.blockId;
            dialog.accept();
        });
        contentGrid->addWidget(blockIcon, blockIndex / 3, blockIndex % 3);
        ++blockIndex;
    }
    if (blockIndex == 0) {
        contentGrid->addWidget(new QLabel("Empty chest.", contents), 0, 0);
    }
    QDialogButtonBox *buttons = new QDialogButtonBox(&dialog);
    QPushButton *skipButton = buttons->addButton(chest.forcedPick ? "Leave" : "Skip", QDialogButtonBox::RejectRole);
    Q_UNUSED(skipButton);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(summary);
    layout->addWidget(contents);
    layout->addWidget(buttons);
    dialog.resize(520, 360);

    QString picked;
    if (dialog.exec() == QDialog::Accepted) {
        if (!selectedBlockId.isEmpty()) {
            if (gameEngine.takeFromChest(chestId, selectedBlockId)) {
                picked = selectedBlockId;
                if (chest.forcedPick) {
                    gameEngine.m_locked = false;
                }
                syncFromEngineState();
            } else {
                QMessageBox::warning(&dialog, "Chest", "Unable to take this code block.");
            }
        }
        ui->combatLogLabel->setText(picked.isEmpty()
                                        ? "No new block was picked."
                                        : QString("Picked: %1").arg(picked));
    } else {
        ui->combatLogLabel->setText(chest.forcedPick ? "Forced chest closed. Returned to the previous tile." : "Chest skipped.");
    }

    if (chest.forcedPick && picked.isEmpty() && gameEngine.m_map) {
        gameEngine.exitChest(chestId);
        syncFromEngineState();
    }
    refreshGameUi();
}

void MainWindow::handleMonster(const QString &monsterId)
{
    seenMonsters.insert(monsterId);
    if (defeatedMonsters.contains(monsterId)) {
        ui->combatLogLabel->setText("This enemy is already defeated.");
        return;
    }

    const Monster monster = monsterByTile(monsterId);
    const bool friendlyEncounter = monster.spaces.isEmpty();
    playBgm(monsterId == "boss" ? "assets/audio/bgm_boss.mp3" : "assets/audio/bgm_combat.wav");
    QDialog dialog(this);
    dialog.setWindowTitle(monsterId == "boss" ? "Boss Encounter" : "Monster Encounter");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    const QString encounterName = monster.nickname.isEmpty() ? monster.name : monster.nickname;
    QLabel *title = new QLabel(monsterId == "boss" || monster.type.compare("boss", Qt::CaseInsensitive) == 0
                                   ? QString("%1  Boss").arg(encounterName)
                                   : encounterName,
                               &dialog);
    title->setWordWrap(true);
    QLabel *hero = new QLabel(&dialog);
    hero->setFixedSize(180, 220);
    hero->setAlignment(Qt::AlignCenter);
    hero->setPixmap(QPixmap(":/images/assets/jibao.png").scaled(hero->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QScrollArea *bagScroll = new QScrollArea(&dialog);
    bagScroll->setWidgetResizable(true);
    bagScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    bagScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bagScroll->setFixedHeight(100);
    bagScroll->setStyleSheet("QScrollArea { background: rgba(9, 13, 18, 230); border: 1px solid #354255; border-radius: 7px; }");
    QWidget *bagStrip = new QWidget(bagScroll);
    QHBoxLayout *bagIconRow = new QHBoxLayout(bagStrip);
    bagIconRow->setContentsMargins(12, 10, 12, 10);
    bagIconRow->setSpacing(10);
    std::function<void()> refreshCombatBag = [this, bagIconRow, bagStrip]() {
        clearLayout(bagIconRow);
        for (const QString &blockId : bagBlocks) {
            CodeBlockIcon *blockIcon = new CodeBlockIcon(blockId, codeBlockIconPath(blockId), bagStrip);
            installHoverPopup(blockIcon, codeBlockPopupHtml(codeForBlock(blockId), typeForBlock(blockId)));
            bagIconRow->addWidget(blockIcon);
        }
        bagIconRow->addStretch();
    };
    refreshCombatBag();
    bagScroll->setWidget(bagStrip);

    QScrollArea *codeScroll = new QScrollArea(&dialog);
    codeScroll->setWidgetResizable(true);
    codeScroll->setMinimumHeight(330);
    codeScroll->setStyleSheet("QScrollArea { background: #111827; border: 1px solid #3a4658; border-radius: 7px; }");
    QFrame *codePanel = new QFrame(codeScroll);
    codePanel->setStyleSheet("QFrame { background: #111827; }");
    QVBoxLayout *codeLayout = new QVBoxLayout(codePanel);
    codeLayout->setContentsMargins(16, 14, 16, 14);
    codeLayout->setSpacing(3);

    QMap<QString, int> spaceIndexById;
    for (int i = 0; i < monster.spaces.size(); ++i) {
        spaceIndexById.insert(monster.spaces.at(i).spaceId, i);
    }

    const QStringList templateLines = monster.codeTemplate.split('\n');
    const QRegularExpression tokenPattern("\\$([^$]+)\\$");
    QSet<QString> hiddenClueIds;
    if (currentLevelIndex >= 0 && currentLevelIndex < levels.size()) {
        const LevelData &level = levels.at(currentLevelIndex);
        for (const QString &line : templateLines) {
            QRegularExpressionMatchIterator clueMatches = tokenPattern.globalMatch(line);
            while (clueMatches.hasNext()) {
                const QString tokenId = clueMatches.next().captured(1);
                if (!spaceIndexById.contains(tokenId)
                    && level.clues.contains(tokenId)
                    && !collectedClues.contains(tokenId)) {
                    hiddenClueIds.insert(tokenId);
                }
            }
        }
        for (const QString &clueId : monster.referencedClues) {
            if (level.clues.contains(clueId) && !collectedClues.contains(clueId)) {
                hiddenClueIds.insert(clueId);
            }
        }
    }
    for (const QString &line : templateLines) {
        QWidget *lineWidget = new QWidget(codePanel);
        QHBoxLayout *lineLayout = new QHBoxLayout(lineWidget);
        lineLayout->setContentsMargins(0, 0, 0, 0);
        lineLayout->setSpacing(0);
        int cursor = 0;
        QRegularExpressionMatchIterator matches = tokenPattern.globalMatch(line);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            const int start = match.capturedStart();
            if (start > cursor) {
                QLabel *text = new QLabel(line.mid(cursor, start - cursor), lineWidget);
                text->setTextFormat(Qt::PlainText);
                text->setStyleSheet("QLabel { color: #f9f1d0; font-family: Consolas, 'Cascadia Mono', monospace; font-size: 16px; background: transparent; }");
                lineLayout->addWidget(text, 0, Qt::AlignVCenter);
            }

            const QString tokenId = match.captured(1);
            if (spaceIndexById.contains(tokenId)) {
                CodeDropSlot *slot = new CodeDropSlot(lineWidget);
                slot->setTextProvider([this](const QString &blockId) {
                    return formatCodeBlockForDisplay(codeForBlock(blockId));
                });
                slot->setOnChanged([this, slot, tokenId, &dialog, refreshCombatBag]() {
                    const QString blockId = slot->property("blockId").toString();
                    const QString previousBlockId = slot->property("previousBlockId").toString();
                    if (blockId == previousBlockId) {
                        return;
                    }
                    if (!gameEngine.fillSpace(tokenId, blockId)) {
                        if (previousBlockId.isEmpty()) {
                            slot->clearBlock();
                        } else {
                            slot->setBlock(previousBlockId, formatCodeBlockForDisplay(codeForBlock(previousBlockId)));
                        }
                        QMessageBox::warning(&dialog, "Wrong Fill", "Fill space with this block.");
                        return;
                    }
                    syncFromEngineState();
                    refreshCombatBag();
                });
                lineLayout->addWidget(slot, 0, Qt::AlignVCenter);
            } else {
                const bool unlocked = collectedClues.contains(tokenId);
                QLabel *clue = new QLabel(lineWidget);
                const QString clueText = unlocked ? formatClueForCode(levels.at(currentLevelIndex).clues.value(tokenId).val) : hiddenCodeMask();
                clue->setText(clueText);
                clue->setTextFormat(Qt::PlainText);
                clue->setWordWrap(false);
                clue->setStyleSheet(unlocked
                                        ? "QLabel { color: #a8f4bf; font-family: Consolas, monospace; font-size: 16px; background: rgba(10, 38, 47, 150); border-radius: 5px; padding: 2px 8px; }"
                                        : "QLabel { color: rgba(249, 241, 208, 80); font-family: Consolas, monospace; font-size: 16px; background: rgba(26, 29, 38, 210); border: 1px solid rgba(120, 130, 145, 120); border-radius: 5px; padding: 2px 8px; }");
                lineLayout->addWidget(clue, 0, clueText.contains('\n') ? Qt::AlignTop : Qt::AlignVCenter);
            }
            cursor = match.capturedEnd();
        }
        if (cursor < line.size()) {
            QLabel *text = new QLabel(line.mid(cursor), lineWidget);
            text->setTextFormat(Qt::PlainText);
            text->setStyleSheet("QLabel { color: #f9f1d0; font-family: Consolas, 'Cascadia Mono', monospace; font-size: 16px; background: transparent; }");
            lineLayout->addWidget(text, 0, Qt::AlignVCenter);
        }
        lineLayout->addStretch(1);
        codeLayout->addWidget(lineWidget);
    }
    codeLayout->addStretch(1);
    codeScroll->setWidget(codePanel);

    QDialogButtonBox *buttons = new QDialogButtonBox(&dialog);
    QPushButton *submitButton = buttons->addButton("Submit fill", QDialogButtonBox::ActionRole);
    if (friendlyEncounter) {
        submitButton->setEnabled(false);
        submitButton->setText("Friendly");
        submitButton->setToolTip("This encounter has no fill space.");
    } else if (!hiddenClueIds.isEmpty()) {
        submitButton->setEnabled(false);
        submitButton->setText("Clues missing");
        submitButton->setToolTip("Reveal every clue linked to this encounter before submitting.");
    }
    buttons->addButton("Exit", QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    bool wonBoss = false;
    connect(submitButton, &QPushButton::clicked, this, [this, &dialog, &wonBoss, monsterId]() {
        if (!gameEngine.m_combat) {
            QMessageBox::warning(&dialog, "Wrong Fill", "No active combat.");
            return;
        }
        const bool isBossCombat = gameEngine.m_combat->isBoss();
        const CombatResult result = gameEngine.submitCombat();
        if (result.resultType == "count_error") {
            QMessageBox::warning(&dialog, "Wrong Fill", "Fill every blank before submitting.");
            return;
        }
        if (result.resultType == "space_error") {
            QMessageBox::warning(&dialog, "Wrong Fill", "Fill space with this block.");
            return;
        }
        if (result.resultType != "success") {
            QMessageBox::warning(&dialog, "Wrong Fill", "Combat submit failed.");
            return;
        }
        wonBoss = isBossCombat;
        if (wonBoss) {
            completedStageIndexes.insert(currentLevelIndex);
            gameEngine.m_save.Clear(currentLevelIndex);
            gameEngine.m_save.Save();
        }
        syncFromEngineState();
        refreshGameUi();
        const Monster clearedMonster = monsterByTile(monsterId);
        const QString clearedName = clearedMonster.nickname.isEmpty() ? clearedMonster.name : clearedMonster.nickname;
        ui->combatLogLabel->setText(wonBoss ? levels.at(currentLevelIndex).endText : QString("%1 defeated.").arg(clearedName));
        dialog.accept();
    });
    QHBoxLayout *combatTop = new QHBoxLayout();
    combatTop->addWidget(hero);
    combatTop->addWidget(codeScroll, 1);
    layout->addWidget(title);
    layout->addLayout(combatTop);

    layout->addWidget(bagScroll);
    layout->addWidget(buttons);
    dialog.resize(860, 660);
    dialog.exec();
    if (gameEngine.m_combat) {
        gameEngine.exitCombat();
    }
    if (wonBoss) {
        showVictorySettlement();
    } else {
        syncBgmToCurrentPage();
    }
}

QString MainWindow::renderMonsterCode(const Monster &monster) const
{
    QSet<QString> spaceIds;
    for (const Space &space : monster.spaces) {
        spaceIds.insert(space.spaceId);
    }
    const LevelData &level = levels.at(currentLevelIndex);
    return replaceCodeTemplateTokens(monster.codeTemplate, [this, &level, &spaceIds](const QString &tokenId) {
        if (spaceIds.contains(tokenId)) {
            return QString("____________");
        }
        return collectedClues.contains(tokenId)
                   ? formatClueForCode(level.clues.value(tokenId).val)
                   : hiddenCodeMask();
    });
}

QString MainWindow::renderMonsterCodeHtml(const Monster &monster) const
{
    QSet<QString> spaceIds;
    for (const Space &space : monster.spaces) {
        spaceIds.insert(space.spaceId);
    }

    QString html;
    const LevelData &level = levels.at(currentLevelIndex);
    const QStringList lines = monster.codeTemplate.split('\n');
    const QRegularExpression tokenPattern("\\$([^$]+)\\$");
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString &line = lines.at(lineIndex);
        int cursor = 0;
        QRegularExpressionMatchIterator matches = tokenPattern.globalMatch(line);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            const int start = match.capturedStart();
            html += line.mid(cursor, start - cursor).toHtmlEscaped();

            const QString tokenId = match.captured(1);
            if (spaceIds.contains(tokenId)) {
                html += QString("<span style=\"%1\">              </span>").arg(codeTokenStyle());
            } else {
                const bool unlocked = collectedClues.contains(tokenId);
                QString value = unlocked ? formatClueForCode(level.clues.value(tokenId).val) : hiddenCodeMask();
                value = indentContinuationLines(value, QString(start, QLatin1Char(' '))).toHtmlEscaped();
                value.replace('\t', "    ");
                const QString style = unlocked
                                          ? codeTokenStyle()
                                          : QString("display:inline-block;padding:3px 8px;margin:0 3px;border:1px solid #5e6470;border-radius:5px;background:#1a1d26;color:#59606c;font-weight:700;");
                html += QString("<span style=\"%1\">%2</span>").arg(style, value);
            }
            cursor = match.capturedEnd();
        }
        html += line.mid(cursor).toHtmlEscaped();
        if (lineIndex + 1 < lines.size()) {
            html += '\n';
        }
    }
    html.replace('\t', "    ");
    return QString(
               "<pre style=\"font-family:'Cascadia Mono','Consolas','Courier New',monospace;"
               "font-size:14px;line-height:1.58;color:#fff4cf;background:#0b1320;"
               "padding:16px 18px;margin:0;white-space:pre-wrap;tab-size:4;\">%1</pre>")
        .arg(html);
}

QString MainWindow::codeForBlock(const QString &blockId) const
{
    if (gameEngine.m_combat) {
        for (const CodeBlock &block : gameEngine.m_combat->filledCodes()) {
            if (block.blockId == blockId) {
                return block.code;
            }
        }
    }
    if (knownCodeBlocks.contains(blockId)) {
        return knownCodeBlocks.value(blockId).code;
    }
    if (currentLevelIndex >= 0 && currentLevelIndex < levels.size()) {
        const LevelData &level = levels.at(currentLevelIndex);
        for (const Chest &chest : level.chests) {
            if (chest.blocks.contains(blockId)) {
                return chest.blocks.value(blockId).code;
            }
        }
    }
    return blockId;
}

QString MainWindow::typeForBlock(const QString &blockId) const
{
    const QString type = codeBlockForId(blockId).type.trimmed().toLower();
    if (type == "class" || type == "function" || type == "natural") {
        return type;
    }
    return "natural";
}

CodeBlock MainWindow::codeBlockForId(const QString &blockId) const
{
    if (gameEngine.m_combat) {
        for (const CodeBlock &block : gameEngine.m_combat->filledCodes()) {
            if (block.blockId == blockId) {
                return block;
            }
        }
    }
    if (knownCodeBlocks.contains(blockId)) {
        return knownCodeBlocks.value(blockId);
    }
    if (currentLevelIndex >= 0 && currentLevelIndex < levels.size()) {
        const LevelData &level = levels.at(currentLevelIndex);
        for (const Chest &chest : level.chests) {
            if (chest.blocks.contains(blockId)) {
                return chest.blocks.value(blockId);
            }
        }
    }
    CodeBlock fallback;
    fallback.blockId = blockId;
    fallback.code = blockId;
    fallback.type = "natural";
    return fallback;
}

QString MainWindow::codeBlockIconPath(const QString &blockId) const
{
    const QString type = typeForBlock(blockId);
    if (type == "class") {
        return ":/images/assets/class.png";
    }
    if (type == "function") {
        return ":/images/assets/function.png";
    }
    return ":/images/assets/natural.png";
}

bool MainWindow::chestHasAvailableBlocks(const QString &chestId) const
{
    if (gameEngine.m_bag) {
        return !gameEngine.m_bag->chestIsEmpty(chestId);
    }

    return false;
}

Monster MainWindow::monsterByTile(const QString &tileId) const
{
    if (tileId == "boss") {
        return levels.at(currentLevelIndex).boss;
    }
    return levels.at(currentLevelIndex).monsters.value(tileId);
}
