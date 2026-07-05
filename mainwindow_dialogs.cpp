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
#include <QShortcut>
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

class DialogCloseInputFilter : public QObject
{
public:
    explicit DialogCloseInputFilter(QDialog *dialog)
        : QObject(dialog)
        , m_dialog(dialog)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);
        if (!m_dialog || !m_dialog->isVisible()) {
            return false;
        }
        if (event->type() == QEvent::KeyPress) {
            m_dialog->reject();
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::RightButton) {
                m_dialog->reject();
                return true;
            }
        }
        return false;
    }

private:
    QDialog *m_dialog = nullptr;
};

static bool settlementIsExLevel(const LevelMeta &meta)
{
    const QString type = meta.levelType.trimmed().toLower();
    const QString name = meta.levelName.trimmed().toLower();
    return type == "ex" || type == "extra" || type.startsWith("ex_") || type.startsWith("ex-")
           || name == "ex" || name.startsWith("ex_") || name.startsWith("ex-")
           || name.startsWith("extra_") || name.startsWith("extra-");
}

static QString settlementStageLabel(const QVector<LevelMeta> &metas, int levelIndex)
{
    if (levelIndex < 0) {
        return QString();
    }

    const auto targetIt = std::find_if(metas.cbegin(), metas.cend(), [levelIndex](const LevelMeta &meta) {
        return meta.levelIndex == levelIndex;
    });

    if (targetIt == metas.cend()) {
        return QString("Stage %1").arg(levelIndex + 1);
    }

    const bool targetIsEx = settlementIsExLevel(*targetIt);
    int visibleIndex = 0;
    for (const LevelMeta &meta : metas) {
        if (settlementIsExLevel(meta) == targetIsEx) {
            ++visibleIndex;
        }
        if (meta.levelIndex == levelIndex) {
            break;
        }
    }

    return targetIsEx ? QString("EX-%1").arg(visibleIndex)
                      : QString("Stage %1").arg(visibleIndex);
}

static QString settlementStageBadgeLabel(const QVector<LevelMeta> &metas, int levelIndex)
{
    const QString label = settlementStageLabel(metas, levelIndex);
    return label.startsWith("Stage ") ? label.mid(QString("Stage ").size()) : label;
}

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

    int iconSlots = 10;
    if (currentLevelIndex >= 0 && currentLevelIndex < levels.size()) {
        iconSlots = qMax(levels.at(currentLevelIndex).bagSize, codeItems.size());
    }
    const int columns = qBound(1, qMin(5, iconSlots), 5);
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
            connect(icon, &QToolButton::clicked, &dialog, [this, &dialog, blockId, code, icon]() {
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
                QObject::connect(discardButton, &QPushButton::clicked, &detail, [this, &detail, blockId, icon, discardButton]() {
                    if (!gameEngine.discardBlock(blockId)) {
                        discardButton->setEnabled(false);
                        discardButton->setText("Locked");
                        discardButton->setToolTip("This code block cannot be discarded.");
                        playSfx("assets/audio/sfx_error.wav");
                        return;
                    }
                    syncFromEngineState();
                    icon->setIcon(QIcon());
                    icon->setText("-");
                    icon->setEnabled(false);
                    detail.accept();
                });
                detailLayout->addWidget(codeView);
                detailLayout->addWidget(close);
                DialogCloseInputFilter *closeInputFilter = new DialogCloseInputFilter(&detail);
                qApp->installEventFilter(closeInputFilter);
                detail.resize(520, 360);
                detail.exec();
            });
        } else {
            icon->setIcon(QIcon());
            icon->setText("-");
            icon->setEnabled(false);
        }
        grid->addWidget(icon, i / columns, i % columns);
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QShortcut *bagShortcut = new QShortcut(QKeySequence(Qt::Key_B), &dialog);
    connect(bagShortcut, &QShortcut::activated, &dialog, &QDialog::reject);
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
        QString codeHtml = renderMonsterCodeHtml(monster);
        const QString defeatedCode = gameEngine.m_map ? gameEngine.m_map->defeatedCode(monsterId) : QString();
        if (!defeatedCode.isEmpty()) {
            codeHtml = QString(
                           "<pre style=\"font-family:'Cascadia Mono','Consolas','Courier New',monospace;"
                           "font-size:14px;line-height:1.58;color:#fff4cf;background:#0b1320;"
                           "padding:16px 18px;margin:0;white-space:pre-wrap;tab-size:4;\">%1</pre>")
                           .arg(codeBlockHtml(defeatedCode));
        }
        codeView->setHtml(codeHtml);
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
            QLabel *io = new QLabel(QString("Expected Input: %1\nExpected Output: %2")
                                        .arg(boss.input.isEmpty() ? "None" : boss.input,
                                             boss.output.isEmpty() ? "None" : boss.output),
                                    infoPanel);
            io->setStyleSheet("QLabel { color: #a8f4bf; font-family: 'Consolas', monospace; font-size: 14px; }");
            io->setWordWrap(true);
            infoLayout->addWidget(io);
        }

        boardLayout->addWidget(infoPanel, 1);
        detailRoot->addWidget(board);
        QDialogButtonBox *detailButtons = new QDialogButtonBox(QDialogButtonBox::Close, &detail);
        connect(detailButtons, &QDialogButtonBox::rejected, &detail, &QDialog::reject);
        detailRoot->addWidget(detailButtons);
        DialogCloseInputFilter *closeInputFilter = new DialogCloseInputFilter(&detail);
        qApp->installEventFilter(closeInputFilter);
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
                                           friendly ? "Friendly" : (defeated ? "Defeated" : "Not Defeated")));
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
    auto addPageShortcut = [&dialog](int key, QPushButton *button) {
        QShortcut *shortcut = new QShortcut(QKeySequence(key), &dialog);
        QObject::connect(shortcut, &QShortcut::activated, button, &QPushButton::click);
    };
    addPageShortcut(Qt::Key_A, prevButton);
    addPageShortcut(Qt::Key_Left, prevButton);
    addPageShortcut(Qt::Key_D, nextButton);
    addPageShortcut(Qt::Key_Right, nextButton);
    refreshPage();

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QShortcut *manualShortcut = new QShortcut(QKeySequence(Qt::Key_M), &dialog);
    connect(manualShortcut, &QShortcut::activated, &dialog, &QDialog::reject);
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

    playSfx("assets/audio/sfx_success.wav");
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
    const QVector<LevelMeta> levelMetas = gameEngine.levelList();
    QLabel *stageLine = new QLabel(QString("%1 Cleared").arg(settlementStageLabel(levelMetas, currentLevelIndex)), summaryPanel);
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
        for (int levelIndex : unlockedThisClear) {
            const QString stageId = settlementStageBadgeLabel(levelMetas, levelIndex);
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

void MainWindow::showCombatSettlement(const QString &defeatedName,
                                      const CombatResult &result,
                                      const QMap<QString, QString> &usedBlockCodes,
                                      bool bossDefeated)
{
    QDialog dialog(this);
    dialog.setWindowTitle("Combat Settlement");
    dialog.setModal(true);

    QVBoxLayout *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);

    QFrame *panel = new QFrame(&dialog);
    panel->setObjectName("combatSettlementPanel");
    panel->setStyleSheet(
        "QFrame#combatSettlementPanel { background: rgba(7, 10, 15, 242); border: 2px solid rgba(215, 176, 106, 210); border-radius: 8px; }"
        "QLabel { color: #f9f1d0; background: transparent; }"
    );
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    QLabel *title = new QLabel(QString("%1 Defeated").arg(defeatedName.isEmpty() ? "Enemy" : defeatedName), panel);
    title->setAlignment(Qt::AlignCenter);
    title->setWordWrap(true);
    title->setStyleSheet(
        "QLabel { color: #ffe082; font-family: Georgia, 'Times New Roman'; font-size: 34px; font-weight: 900;"
        "border: 1px solid rgba(255, 224, 130, 115); border-radius: 6px; padding: 10px;"
        "background: rgba(54, 26, 12, 225); }"
    );
    layout->addWidget(title);

    auto addCodeSection = [panel, layout](const QString &heading, const QStringList &codes) {
        QLabel *label = new QLabel(heading, panel);
        label->setStyleSheet("QLabel { color: #4feaff; font-size: 17px; font-weight: 900; }");
        layout->addWidget(label);

        QTextBrowser *view = new QTextBrowser(panel);
        view->setReadOnly(true);
        view->setContextMenuPolicy(Qt::NoContextMenu);
        view->setTextInteractionFlags(Qt::NoTextInteraction);
        view->setMinimumHeight(110);
        view->setStyleSheet(
            "QTextBrowser { color: #fff4cf; background: rgba(10, 16, 25, 245);"
            "border: 1px solid rgba(79, 234, 255, 120); border-radius: 6px; padding: 8px; }"
        );
        QString html;
        if (codes.isEmpty()) {
            html = "<span style=\"color:#9da7b7;\">None</span>";
        } else {
            QStringList blocks;
            for (const QString &code : codes) {
                blocks << QString("<pre style=\"font-family:'Cascadia Mono','Consolas','Courier New',monospace;"
                                  "font-size:14px;line-height:1.45;margin:0 0 10px 0;white-space:pre-wrap;\">%1</pre>")
                              .arg(codeBlockHtml(code));
            }
            html = blocks.join("<hr style=\"border:0;border-top:1px solid rgba(215,176,106,95);\">");
        }
        view->setHtml(html);
        layout->addWidget(view);
    };

    QStringList consumedCodes;
    for (const QString &blockId : result.usedBlocks) {
        if (usedBlockCodes.contains(blockId)) {
            consumedCodes << usedBlockCodes.value(blockId);
        }
    }
    addCodeSection("Consumed Code Blocks", consumedCodes);

    QStringList generatedCodes;
    if (!bossDefeated && !result.synthesizedBlock.code.trimmed().isEmpty()) {
        generatedCodes << result.synthesizedBlock.code;
    }
    addCodeSection("Generated Code Block", generatedCodes);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, panel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    root->addWidget(panel);
    dialog.resize(720, 680);
    dialog.exec();
}

void MainWindow::handleChest(const QString &chestId, bool viewOnly, bool lockedByChest)
{
    const Chest chest = levels.at(currentLevelIndex).chests.value(chestId);
    const bool bundlePick = currentLevelIndex >= 0
                            && currentLevelIndex < levels.size()
                            && levels.at(currentLevelIndex).specialTags.contains("bundle_pick");
    const bool activeForcedChest = !viewOnly
                                   && chest.forcedPick
                                   && lockedByChest
                                   && tileAt(playerRow, playerColumn) == chestId;
    if (!gameEngine.m_bag || (!viewOnly && !chestHasAvailableBlocks(chestId))) {
        ui->combatLogLabel->setText("This chest is already empty.");
        playSfx("assets/audio/sfx_error.wav");
        refreshGameUi();
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Chest");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *chestImage = new QLabel(&dialog);
    chestImage->setAlignment(Qt::AlignCenter);
    const QString chestPixmapPath = chest.forcedPick ? ":/images/assets/forced_chest.png" : ":/images/assets/unforce_chest.png";
    chestImage->setPixmap(QPixmap(chestPixmapPath).scaled(170, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QLabel *summary = new QLabel(QString("强制拾取: %1    可多次拾取: %2")
                                     .arg(chest.forcedPick ? "是" : "否",
                                          chest.repeat ? "是" : "否"),
                                 &dialog);
    summary->setAlignment(Qt::AlignCenter);
    summary->setStyleSheet("QLabel { color: #f9f1d0; background: rgba(31, 38, 50, 225); border: 1px solid #4a596d; border-radius: 7px; padding: 12px; font-size: 16px; font-weight: 800; }");

    QScrollArea *contentScroll = new QScrollArea(&dialog);
    contentScroll->setWidgetResizable(true);
    contentScroll->setMinimumHeight(180);
    contentScroll->setStyleSheet("QScrollArea { background: rgba(8, 11, 16, 235); border: 1px solid #354255; border-radius: 7px; }");
    QString selectedBlockId;
    bool selectedBundle = false;
    bool detailedMode = chestDetailedByDefault;
    const QVector<CodeBlock> remainingBlocks = gameEngine.m_bag->blocksRemaining(chestId);

    std::function<void()> renderChestContents = [&]() {
        if (QWidget *oldWidget = contentScroll->takeWidget()) {
            oldWidget->deleteLater();
        }

        QWidget *contents = new QWidget(contentScroll);
        contents->setStyleSheet("QWidget { background: transparent; }");
        if (remainingBlocks.isEmpty()) {
            QVBoxLayout *emptyLayout = new QVBoxLayout(contents);
            QLabel *empty = new QLabel("Empty chest.", contents);
            empty->setAlignment(Qt::AlignCenter);
            empty->setStyleSheet("QLabel { color: #9aa5b5; font-size: 15px; padding: 24px; }");
            emptyLayout->addWidget(empty);
            contentScroll->setWidget(contents);
            return;
        }

        if (!detailedMode) {
            QGridLayout *contentGrid = new QGridLayout(contents);
            contentGrid->setContentsMargins(14, 14, 14, 14);
            contentGrid->setSpacing(12);
            int blockIndex = 0;
            for (const CodeBlock &block : remainingBlocks) {
                QToolButton *blockIcon = new QToolButton(contents);
                blockIcon->setFixedSize(128, 68);
                blockIcon->setToolButtonStyle(Qt::ToolButtonIconOnly);
                blockIcon->setIcon(QIcon(codeBlockIconPath(block.blockId)));
                blockIcon->setIconSize(QSize(58, 58));
                blockIcon->setToolTip(formatCodeBlockForDisplay(block.code));
                installHoverPopup(blockIcon, codeBlockPopupHtml(block.code, typeForBlock(block.blockId)));
                blockIcon->setStyleSheet(viewOnly
                                             ? "QToolButton { background: #15242b; border: 2px solid #7c6a45; border-radius: 5px; color: #9d9072; font-weight: 700; }"
                                             : "QToolButton { background: #15242b; border: 2px solid #d7b06a; border-radius: 5px; color: #ffe8ad; font-weight: 700; }"
                                               "QToolButton:hover { border-color: #49e6ff; color: white; }");
                if (!viewOnly && !bundlePick) {
                    connect(blockIcon, &QToolButton::clicked, &dialog, [&dialog, &selectedBlockId, block]() {
                        selectedBlockId = block.blockId;
                        dialog.accept();
                    });
                }
                contentGrid->addWidget(blockIcon, blockIndex / 3, blockIndex % 3);
                ++blockIndex;
            }
        } else {
            QVBoxLayout *detailLayout = new QVBoxLayout(contents);
            detailLayout->setContentsMargins(12, 12, 12, 12);
            detailLayout->setSpacing(10);
            for (const CodeBlock &block : remainingBlocks) {
                QFrame *row = new QFrame(contents);
                row->setStyleSheet("QFrame { background: rgba(18, 25, 34, 230); border: 1px solid #4a596d; border-radius: 7px; }");
                QHBoxLayout *rowLayout = new QHBoxLayout(row);
                rowLayout->setContentsMargins(12, 10, 12, 10);
                rowLayout->setSpacing(12);

                QLabel *icon = new QLabel(row);
                icon->setFixedSize(74, 74);
                icon->setAlignment(Qt::AlignCenter);
                icon->setPixmap(QPixmap(codeBlockIconPath(block.blockId)).scaled(62, 62, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                icon->setStyleSheet("QLabel { background: rgba(7, 13, 19, 185); border: 2px solid #d7b75f; border-radius: 6px; }");

                QTextBrowser *details = new QTextBrowser(row);
                details->setReadOnly(true);
                details->setContextMenuPolicy(Qt::NoContextMenu);
                details->setTextInteractionFlags(Qt::NoTextInteraction);
                details->setMinimumHeight(112);
                details->setHtml(QString(
                                     "<div style=\"font-family:'Microsoft YaHei UI'; color:#f9f1d0; font-size:13px;\">"
                                     "<pre style=\"font-family:Consolas,'Cascadia Mono',monospace; font-size:13px; line-height:1.45;"
                                     "color:#fff4cf; background:#0b1320; padding:8px; margin:0; white-space:pre-wrap;\">%1</pre>"
                                     "</div>")
                                     .arg(codeBlockHtml(block.code)));
                details->setStyleSheet("QTextBrowser { background: transparent; border: none; }");

                rowLayout->addWidget(icon, 0, Qt::AlignTop);
                rowLayout->addWidget(details, 1);
                if (!viewOnly && !bundlePick) {
                    QPushButton *takeButton = new QPushButton("Take", row);
                    takeButton->setFixedSize(82, 36);
                    connect(takeButton, &QPushButton::clicked, &dialog, [&dialog, &selectedBlockId, block]() {
                        selectedBlockId = block.blockId;
                        dialog.accept();
                    });
                    rowLayout->addWidget(takeButton, 0, Qt::AlignVCenter);
                }
                detailLayout->addWidget(row);
            }
            detailLayout->addStretch(1);
        }
        contentScroll->setWidget(contents);
    };
    renderChestContents();

    QDialogButtonBox *buttons = new QDialogButtonBox(&dialog);
    QPushButton *modeButton = buttons->addButton(detailedMode ? "Compact" : "Detailed", QDialogButtonBox::ActionRole);
    QPushButton *takeBundleButton = nullptr;
    if (!viewOnly && bundlePick) {
        takeBundleButton = buttons->addButton("Take All", QDialogButtonBox::AcceptRole);
        connect(takeBundleButton, &QPushButton::clicked, &dialog, [&dialog, &selectedBundle]() {
            selectedBundle = true;
            dialog.accept();
        });
    }
    QPushButton *skipButton = buttons->addButton(viewOnly ? "Close" : (activeForcedChest ? "Leave" : "Skip"), QDialogButtonBox::RejectRole);
    Q_UNUSED(skipButton);
    connect(modeButton, &QPushButton::clicked, &dialog, [&]() {
        detailedMode = !detailedMode;
        modeButton->setText(detailedMode ? "Compact" : "Detailed");
        renderChestContents();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(chestImage);
    layout->addWidget(summary);
    layout->addWidget(contentScroll);
    layout->addWidget(buttons);
    dialog.resize(detailedMode ? 760 : 560, detailedMode ? 560 : 420);

    QString picked;
    if (dialog.exec() == QDialog::Accepted) {
        if (viewOnly) {
            ui->combatLogLabel->setText("Chest is out of reach.");
            refreshGameUi();
            return;
        }
        if (selectedBundle) {
            if (gameEngine.takeBundleFromChest(chestId)) {
                picked = chestId;
                syncFromEngineState();
                playSfx("assets/audio/sfx_event.wav");
            } else {
                playSfx("assets/audio/sfx_error.wav");
                QMessageBox::warning(&dialog, "Chest", "Unable to take all code blocks.");
            }
        } else if (!selectedBlockId.isEmpty()) {
            if (gameEngine.takeFromChest(chestId, selectedBlockId)) {
                picked = selectedBlockId;
                syncFromEngineState();
                playSfx("assets/audio/sfx_event.wav");
            } else {
                playSfx("assets/audio/sfx_error.wav");
                QMessageBox::warning(&dialog, "Chest", "Unable to take this code block.");
            }
        }
        ui->combatLogLabel->setText(picked.isEmpty()
                                        ? "No new block was picked."
                                        : "Picked a code block.");
    } else if (viewOnly) {
        ui->combatLogLabel->setText("Chest is out of reach.");
    } else {
        ui->combatLogLabel->setText(activeForcedChest ? "Forced chest closed. Returned to the previous tile." : "Chest skipped.");
    }

    if (activeForcedChest && picked.isEmpty() && gameEngine.m_map) {
        gameEngine.exitChest(chestId);
        syncFromEngineState();
    }
    refreshGameUi();
}

void MainWindow::handleMonster(const QString &monsterId, bool viewOnly)
{
    seenMonsters.insert(monsterId);
    if (defeatedMonsters.contains(monsterId)) {
        ui->combatLogLabel->setText("This enemy is already defeated.");
        playSfx("assets/audio/sfx_error.wav");
        return;
    }

    const Monster monster = monsterByTile(monsterId);
    const bool friendlyEncounter = monster.spaces.isEmpty()&&!(monsterId=="boss"&&gameEngine.m_level->specialTags.contains("friendly_boss"));
    if (!viewOnly) {
        playBgm(monsterId == "boss" ? "assets/audio/bgm_boss.mp3" : "assets/audio/bgm_combat.wav");
    }
    QDialog dialog(this);
    dialog.setWindowTitle(viewOnly ? "Encounter Preview" : (monsterId == "boss" ? "Boss Encounter" : "Monster Encounter"));
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QString encounterName = monster.nickname.isEmpty() ? monster.name : monster.nickname;
    bool capitalizeNext = true;
    for (QChar &ch : encounterName) {
        if (capitalizeNext && ch.isLetter()) {
            ch = ch.toUpper();
            capitalizeNext = false;
        } else if (ch.isSpace() || ch == '-' || ch == '_') {
            capitalizeNext = true;
        }
    }
    QLabel *title = new QLabel(monsterId == "boss" || monster.type.compare("boss", Qt::CaseInsensitive) == 0
                                   ? QString("%1  Boss").arg(encounterName)
                                   : encounterName,
                               &dialog);
    title->setWordWrap(true);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(monsterId == "boss" || monster.type.compare("boss", Qt::CaseInsensitive) == 0
                             ? "QLabel { color: #ffe082; font-size: 27px; font-weight: 900; background: rgba(68, 20, 24, 225); border: 2px solid #ff626e; border-radius: 7px; padding: 9px; }"
                             : "QLabel { color: #f9f1d0; font-size: 23px; font-weight: 900; background: rgba(31, 38, 50, 225); border: 1px solid #5a6a82; border-radius: 7px; padding: 8px; }");
    QLabel *hero = new QLabel(&dialog);
    hero->setFixedSize(180, 220);
    hero->setAlignment(Qt::AlignCenter);
    hero->setPixmap(QPixmap(":/images/assets/jibao.png").scaled(hero->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    hero->setStyleSheet("QLabel { background: rgba(5, 8, 12, 220); border: 1px solid rgba(79, 234, 255, 140); border-radius: 7px; padding: 8px; }");

    auto combatSpritePath = [](const QString &monsterId, const Monster &monster) {
        if (monsterId == "boss" || monster.monsterId == "boss") {
            return QString(":/images/assets/marry_ann.png");
        }
        QString normalizedPic = monster.pic.trimmed();
        normalizedPic.replace('\\', '/');
        const QString fileName = normalizedPic.mid(normalizedPic.lastIndexOf('/') + 1);
        const QStringList candidates = {
            normalizedPic.startsWith(":/") ? normalizedPic : QString(":/images/%1").arg(normalizedPic),
            fileName.isEmpty() ? QString() : QString(":/images/assets/%1").arg(fileName)
        };
        for (const QString &candidate : candidates) {
            if (!candidate.isEmpty() && !QPixmap(candidate).isNull()) {
                return candidate;
            }
        }
        return QString(":/images/assets/jabberwock.png");
    };
    QLabel *enemy = new QLabel(&dialog);
    enemy->setFixedSize(180, 220);
    enemy->setAlignment(Qt::AlignCenter);
    enemy->setPixmap(QPixmap(combatSpritePath(monsterId, monster)).scaled(enemy->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    enemy->setStyleSheet("QLabel { background: rgba(5, 8, 12, 220); border: 1px solid rgba(215, 176, 106, 150); border-radius: 7px; padding: 8px; }");

    QScrollArea *bagScroll = new QScrollArea(&dialog);
    bagScroll->setWidgetResizable(true);
    bagScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    bagScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bagScroll->setFixedHeight(100);
    bagScroll->setStyleSheet("QScrollArea { background: rgba(9, 13, 18, 230); border: 1px solid #354255; border-radius: 7px; }");
    bagScroll->setVisible(!viewOnly);
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

    QString displayTemplate = monster.codeTemplate;
    displayTemplate.replace('\t', "    ");
    const QStringList templateLines = displayTemplate.split('\n');
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
    Synthesis templateInfo;
    try {
        templateInfo = templateBreakdown(monster.codeTemplate);
    } catch (...) {
        // Malformed templates already surfaced at load time; fall back to no indent.
    }
    QFont combatCodeFont("Consolas");
    combatCodeFont.setPointSize(16);
    QFontMetrics combatCodeMetrics(combatCodeFont);
    const int combatLineHeight = qMax(28, combatCodeMetrics.height() + 7);
    QVector<std::function<void()>> slotGeometryUpdaters;

    int templateCellIndex = 0;
    int displayBraceDepth = 0;
    for (const QString &line : templateLines) {
        const int lineBraceDepth = qMax(0, displayBraceDepth - (line.trimmed().startsWith('}') ? 1 : 0));
        const int autoIndentColumns = lineBraceDepth * 4;
        QWidget *lineWidget = new QWidget(codePanel);
        lineWidget->setMinimumHeight(combatLineHeight);
        lineWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        int visualX = 0;
        int maxVisualX = 0;
        int lineVisualHeight = combatLineHeight;
        auto advanceTextSegment = [&](const QString &segment) {
            if (segment.isEmpty()) {
                return;
            }
            int visibleStart = 0;
            while (visibleStart < segment.size() && segment.at(visibleStart).isSpace()) {
                ++visibleStart;
            }
            if (visibleStart > 0) {
                visualX += combatCodeMetrics.horizontalAdvance(segment.left(visibleStart));
            }
            const QString visibleText = segment.mid(visibleStart);
            if (visibleText.isEmpty()) {
                maxVisualX = qMax(maxVisualX, visualX);
                return;
            }
            QLabel *text = new QLabel(visibleText, lineWidget);
            text->setTextFormat(Qt::PlainText);
            text->setFont(combatCodeFont);
            text->setStyleSheet("QLabel { color: #f9f1d0; background: transparent; border: none; padding: 0; }");
            const int width = combatCodeMetrics.horizontalAdvance(visibleText);
            text->setGeometry(visualX, 0, qMax(1, width), combatLineHeight);
            visualX += width;
            maxVisualX = qMax(maxVisualX, visualX);
        };
        int cursor = 0;
        QRegularExpressionMatchIterator matches = tokenPattern.globalMatch(line);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            const int start = match.capturedStart();
            const QString tokenId = match.captured(1);
            const bool isSpace = spaceIndexById.contains(tokenId);
            const int tabCount = (templateCellIndex < templateInfo.cell.size())
                                     ? templateInfo.cell.at(templateCellIndex).tabCount
                                     : 0;
            ++templateCellIndex;

            // For a revealed clue that occupies its own line, move the clue
            // widget to the template indent instead of baking spaces into text.
            bool clueOwnsLine = false;
            int clueOwnsLineIndentColumns = 0;
            QString clueText;
            if (!isSpace) {
                const bool unlocked = collectedClues.contains(tokenId);
                clueText = unlocked ? formatClueForCode(levels.at(currentLevelIndex).clues.value(tokenId).val)
                                    : hiddenCodeMask();
                if (unlocked && tabCount > 0 && clueText.contains('\n')) {
                    const QString prefix = line.mid(cursor, start - cursor);
                    const QString trailing = line.mid(match.capturedEnd());
                    if (prefix.trimmed().isEmpty() && trailing.trimmed().isEmpty()) {
                        clueOwnsLineIndentColumns = tabCount;
                        clueOwnsLine = true;
                    }
                }
            }

            if (start > cursor && !clueOwnsLine) {
                advanceTextSegment(line.mid(cursor, start - cursor));
            } else if (clueOwnsLine && visualX == 0 && clueOwnsLineIndentColumns > 0) {
                visualX = combatCodeMetrics.horizontalAdvance(QString(clueOwnsLineIndentColumns, QLatin1Char(' ')));
                maxVisualX = qMax(maxVisualX, visualX);
            }

            if (isSpace) {
                if (visualX == 0 && start == 0 && autoIndentColumns > 0) {
                    const QString autoIndent(autoIndentColumns, QLatin1Char(' '));
                    visualX = combatCodeMetrics.horizontalAdvance(autoIndent);
                    maxVisualX = qMax(maxVisualX, visualX);
                }
                CodeDropSlot *slot = new CodeDropSlot(lineWidget);
                slot->setAcceptDrops(!viewOnly);
                slot->setToolTip(viewOnly
                                     ? "Preview only."
                                     : (beginnerTipsEnabled() ? "Double-click a filled blank to undo it." : QString()));
                slot->setProperty("visualX", visualX);
                slot->setTextProvider([this, slot, &combatCodeMetrics](const QString &blockId) {
                    const QString formatted = formatCodeBlockForDisplay(codeForBlock(blockId));
                    const int indentColumns = qMax(0, slot->property("visualX").toInt() / qMax(1, combatCodeMetrics.horizontalAdvance(QStringLiteral(" "))));
                    return indentContinuationLines(formatted, QString(indentColumns, QLatin1Char(' ')));
                });
                auto updateSlotGeometry = [slot, lineWidget, codeScroll, combatLineHeight]() {
                    const int availableWidth = codeScroll->viewport()
                                                   ? codeScroll->viewport()->width() - slot->property("visualX").toInt() - 36
                                                   : 520;
                    const int slotWidth = qMax(220, availableWidth);
                    slot->setContentWidthLimit(slotWidth);
                    const QSize slotSize = slot->sizeForContentWidth(slotWidth);
                    const int slotHeight = qMax(combatLineHeight, slotSize.height());
                    slot->setGeometry(slot->property("visualX").toInt(), 0, slotSize.width(), slotHeight);
                    lineWidget->setMinimumHeight(qMax(combatLineHeight, slotHeight));
                    lineWidget->setMinimumWidth(slot->property("visualX").toInt() + slotSize.width() + 8);
                    lineWidget->updateGeometry();
                };
                slotGeometryUpdaters.append(updateSlotGeometry);
                if (!viewOnly) {
                    slot->setOnRemoveRequested([this, slot, tokenId, refreshCombatBag, updateSlotGeometry]() {
                        if (!gameEngine.unfillSpace(tokenId)) {
                            playSfx("assets/audio/sfx_error.wav");
                            return;
                        }
                        slot->clearBlock();
                        updateSlotGeometry();
                        syncFromEngineState();
                        refreshCombatBag();
                        playSfx("assets/audio/sfx_combat.wav");
                    });
                    slot->setOnChanged([this, slot, tokenId, &dialog, refreshCombatBag, updateSlotGeometry, &combatCodeMetrics]() {
                        const QString blockId = slot->property("blockId").toString();
                        const QString previousBlockId = slot->property("previousBlockId").toString();
                        if (blockId == previousBlockId) {
                            return;
                        }
                        if (!gameEngine.fillSpace(tokenId, blockId)) {
                            if (previousBlockId.isEmpty()) {
                                slot->clearBlock();
                            } else {
                                const int indentColumns = qMax(0, slot->property("visualX").toInt() / qMax(1, combatCodeMetrics.horizontalAdvance(QStringLiteral(" "))));
                                slot->setBlock(previousBlockId,
                                               indentContinuationLines(formatCodeBlockForDisplay(codeForBlock(previousBlockId)),
                                                                      QString(indentColumns, QLatin1Char(' '))));
                            }
                            playSfx("assets/audio/sfx_error.wav");
                            QMessageBox::warning(&dialog, "Wrong Fill", "Can't fill space with this block.");
                            return;
                        }
                        updateSlotGeometry();
                        syncFromEngineState();
                        refreshCombatBag();
                        playSfx("assets/audio/sfx_combat.wav");
                    });
                }
                updateSlotGeometry();
                lineVisualHeight = qMax(lineVisualHeight, slot->height());
                visualX += slot->width();
                maxVisualX = qMax(maxVisualX, visualX);
            } else {
                const bool unlocked = collectedClues.contains(tokenId);
                QLabel *clue = new QLabel(lineWidget);
                clue->setText(clueText);
                clue->setTextFormat(Qt::PlainText);
                clue->setWordWrap(false);
                clue->setFont(combatCodeFont);
                clue->setStyleSheet(unlocked
                                        ? "QLabel { color: #a8f4bf; background: rgba(10, 38, 47, 150); border-radius: 5px; padding: 2px 8px; }"
                                        : "QLabel { color: rgba(249, 241, 208, 80); background: rgba(26, 29, 38, 210); border: 1px solid rgba(120, 130, 145, 120); border-radius: 5px; padding: 2px 8px; }");
                clue->adjustSize();
                clue->setGeometry(visualX, 0, clue->width(), qMax(combatLineHeight, clue->height()));
                lineVisualHeight = qMax(lineVisualHeight, clue->height());
                visualX += clue->width();
                maxVisualX = qMax(maxVisualX, visualX);
            }
            cursor = match.capturedEnd();
        }
        if (cursor < line.size()) {
            advanceTextSegment(line.mid(cursor));
        }
        lineWidget->setMinimumHeight(lineVisualHeight);
        lineWidget->setMinimumWidth(maxVisualX + 8);
        codeLayout->addWidget(lineWidget);

        QString structuralLine = line;
        structuralLine.remove(tokenPattern);
        for (const QChar ch : structuralLine) {
            if (ch == '{') {
                ++displayBraceDepth;
            } else if (ch == '}') {
                displayBraceDepth = qMax(0, displayBraceDepth - 1);
            }
        }
    }
    codeLayout->addStretch(1);
    codeScroll->setWidget(codePanel);
    QTimer::singleShot(0, &dialog, [slotGeometryUpdaters]() {
        for (const auto &updateSlotGeometry : slotGeometryUpdaters) {
            updateSlotGeometry();
        }
    });

    QDialogButtonBox *buttons = new QDialogButtonBox(&dialog);
    QPushButton *submitButton = buttons->addButton(viewOnly ? "Close" : "Submit Fill", viewOnly ? QDialogButtonBox::RejectRole : QDialogButtonBox::ActionRole);
    if (viewOnly) {
        submitButton->setToolTip("This encounter is out of reach.");
    } else if (friendlyEncounter) {
        submitButton->setEnabled(false);
        submitButton->setText("Friendly");
        submitButton->setToolTip("This encounter has no fill space.");
    } else if (!hiddenClueIds.isEmpty()) {
        submitButton->setEnabled(false);
        submitButton->setText("Clues Missing");
        submitButton->setToolTip("Reveal every clue linked to this encounter before submitting.");
    }
    if (!viewOnly) {
        buttons->addButton("Exit", QDialogButtonBox::RejectRole);
    }
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    bool wonBoss = false;
    bool hasCombatSettlement = false;
    CombatResult combatSettlementResult;
    QMap<QString, QString> combatSettlementUsedCodes;
    QString combatSettlementName;
    if (!viewOnly) {
        connect(submitButton, &QPushButton::clicked, this, [this, &dialog, &wonBoss, &hasCombatSettlement, &combatSettlementResult, &combatSettlementUsedCodes, &combatSettlementName, monsterId]() {
            playSfx("assets/audio/sfx_combat.wav");
            if (!gameEngine.m_combat) {
                playSfx("assets/audio/sfx_error.wav");
                QMessageBox::warning(&dialog, "Wrong Fill", "No active combat.");
                return;
            }
            const bool isBossCombat = gameEngine.m_combat->isBoss();
            combatSettlementUsedCodes.clear();
            for (const CodeBlock &block : gameEngine.m_combat->filledCodes()) {
                combatSettlementUsedCodes.insert(block.blockId, block.code);
            }
            const CombatResult result = gameEngine.submitCombat();
            if (result.resultType == "count_error") {
                playSfx("assets/audio/sfx_error.wav");
                QMessageBox::warning(&dialog, "Wrong Fill", "Fill every blank before submitting.");
                return;
            }
            if (result.resultType == "space_error") {
                playSfx("assets/audio/sfx_error.wav");
                QMessageBox::warning(&dialog, "Wrong Fill", "Can't fill space with this block.");
                return;
            }
            if (result.resultType != "success") {
                playSfx("assets/audio/sfx_error.wav");
                QMessageBox::warning(&dialog, "Wrong Fill", "Combat submit failed.");
                return;
            }
            playSfx("assets/audio/sfx_success.wav");
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
            combatSettlementResult = result;
            combatSettlementName = clearedName;
            hasCombatSettlement = true;
            ui->combatLogLabel->setText(wonBoss ? levels.at(currentLevelIndex).endText : QString("%1 defeated.").arg(clearedName));
            dialog.accept();
        });
    }
    QHBoxLayout *combatTop = new QHBoxLayout();
    combatTop->addWidget(hero);
    combatTop->addWidget(codeScroll, 1);
    combatTop->addWidget(enemy);
    QLabel *combatHint = new QLabel(viewOnly
                                        ? "Preview only. Move closer to start this encounter."
                                        : "Drag blocks into blanks. Double-click a filled blank to undo it.",
                                    &dialog);
    combatHint->setAlignment(Qt::AlignCenter);
    combatHint->setWordWrap(true);
    combatHint->setVisible(viewOnly || beginnerTipsEnabled());
    combatHint->setStyleSheet("QLabel { color: #fff1c2; background: rgba(9, 13, 18, 210); border: 1px solid rgba(215, 176, 106, 150); border-radius: 6px; padding: 6px 10px; font-size: 13px; font-weight: 800; }");
    layout->addWidget(title);
    layout->addLayout(combatTop);

    layout->addWidget(combatHint);
    if (!viewOnly) {
        layout->addWidget(bagScroll);
    }
    layout->addWidget(buttons);
    dialog.resize(1060, viewOnly ? 560 : 660);
    dialog.exec();
    if (!viewOnly && gameEngine.m_combat) {
        gameEngine.exitCombat();
    }
    const bool skipCombatSettlement = currentLevelIndex >= 0
                                      && currentLevelIndex < levels.size()
                                      && levels.at(currentLevelIndex).specialTags.contains("discard_drops");
    if (!viewOnly && hasCombatSettlement && !skipCombatSettlement) {
        showCombatSettlement(combatSettlementName, combatSettlementResult, combatSettlementUsedCodes, wonBoss);
    }
    if (viewOnly) {
        ui->combatLogLabel->setText("Enemy is out of reach.");
        refreshGameUi();
    } else if (wonBoss) {
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
