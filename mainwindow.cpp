#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mapview.h"

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

#include <functional>

namespace {
void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        if (QLayout *childLayout = item->layout()) {
            clearLayout(childLayout);
            delete childLayout;
        }
        delete item;
    }
}

QStringList fallbackLevelPaths()
{
    return {
        QDir(QCoreApplication::applicationDirPath()).filePath("levels"),
        QDir::current().filePath("levels"),
        QCoreApplication::applicationDirPath(),
        QDir::currentPath()
    };
}

struct StageCard
{
    int levelIndex;
    bool isEx;
    QString title;
    QString subtitle;
    QString hint;
};

QVector<StageCard> stageCatalog()
{
    return {
        {0, false, "Stage 1", "Pointer Gate", "Warm-up path. Learn movement, chests, and code blocks."},
        {1, false, "Stage 2", "Stack Corridor", "A compact dungeon route with stricter block choices."},
        {2, false, "Stage 3", "Array Depths", "Beware the bounds. The boss gate starts watching."},
        {3, false, "Stage 4", "Heap Bridge", "A branching route that asks the player to plan block order."},
        {4, false, "Stage 5", "Lambda Atrium", "Introduce slightly longer fill chains and optional clues."},
        {5, false, "Stage 6", "Graph Lift", "Unlocked after the lower bridge is cleared."},
        {6, false, "Stage 7", "Bit Forge", "A late normal route for denser C++ syntax puzzles."},
        {7, false, "Stage 8", "Memory Crown", "The final normal ascent before the spire core."},
        {8, false, "Stage 9", "Compile Throne", "Normal boss placeholder for the full chapter arc."},
        {9, true, "EX-1", "Template Rift", "Extra challenge route. Designed for later hard levels."},
        {10, true, "EX-2", "Iterator Vault", "Optional branch with denser clue and chest logic."},
        {11, true, "EX-3", "Undefined Core", "A harder fill route with fewer visible hints."},
        {12, true, "EX-4", "Const Labyrinth", "Requires prior EX proof before entry."},
        {13, true, "EX-5", "Overload Gate", "A special branch for advanced C++ mechanics."},
        {14, true, "EX-6", "Shadow Linker", "A locked extension for future JSON content."},
        {15, true, "EX-7", "Metaprogram Pit", "Late EX placeholder with strict topology."},
        {16, true, "EX-8", "Undefined Peak", "The final optional climb."},
        {17, true, "EX-9", "Core Dump", "EX boss placeholder for the chapter finale."}
    };
}

constexpr int stagesPerPage = 3;

QString codeTokenStyle()
{
    return "display:inline-block;"
           "padding:3px 8px;"
           "margin:0 3px;"
           "border:1px solid #49e6ff;"
           "border-radius:5px;"
           "background:#0b2530;"
           "color:#baf8ff;"
           "font-weight:700;";
}

QString hiddenCodeMask()
{
    return "************";
}

QString formatCodeBlockForDisplay(QString code)
{
    code = code.trimmed();
    if (code.isEmpty()) {
        return code;
    }

    code.replace("\r\n", "\n");
    code.replace('\r', '\n');

    const bool alreadyMultiline = code.contains('\n');
    const bool looksCompound = code.contains('{') || code.contains('}') || code.count(';') > 1;
    if (!alreadyMultiline && !looksCompound) {
        return code;
    }

    if (!alreadyMultiline) {
        QString expanded;
        for (int i = 0; i < code.size(); ++i) {
            const QChar ch = code.at(i);
            if (ch == '{') {
                expanded += " {\n";
            } else if (ch == '}') {
                expanded += "\n}";
                if (i + 1 < code.size() && code.at(i + 1) != ';') {
                    expanded += '\n';
                }
            } else if (ch == ';') {
                expanded += ";\n";
            } else {
                expanded += ch;
            }
        }
        code = expanded;
    }

    QStringList formatted;
    int indent = 0;
    for (QString line : code.split('\n')) {
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith('}')) {
            indent = qMax(0, indent - 1);
        }
        formatted << QString(indent * 4, QLatin1Char(' ')) + line;
        if (line.endsWith('{')) {
            ++indent;
        }
    }

    return formatted.join('\n');
}

QString indentContinuationLines(QString text, const QString &continuationIndent)
{
    text.replace("\r\n", "\n");
    text.replace('\r', '\n');
    QStringList lines = text.split('\n');
    for (int i = 1; i < lines.size(); ++i) {
        if (!lines[i].isEmpty()) {
            lines[i].prepend(continuationIndent);
        }
    }
    return lines.join('\n');
}

QString formatClueForCode(const QString &clue)
{
    return formatCodeBlockForDisplay(clue);
}

QString formatClueListLine(const QString &clueId, const QString &clueText)
{
    const QString prefix = QString("%1: ").arg(clueId);
    return prefix + indentContinuationLines(formatClueForCode(clueText), QString(prefix.size(), QLatin1Char(' ')));
}

QString replaceCodeTemplateTokens(const QString &codeTemplate, const std::function<QString(const QString &)> &replacementForToken)
{
    QString rendered;
    const QStringList lines = codeTemplate.split('\n');
    const QRegularExpression tokenPattern("\\$([^$]+)\\$");
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString &line = lines.at(lineIndex);
        int cursor = 0;
        QRegularExpressionMatchIterator matches = tokenPattern.globalMatch(line);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            const int start = match.capturedStart();
            rendered += line.mid(cursor, start - cursor);
            rendered += indentContinuationLines(replacementForToken(match.captured(1)), QString(start, QLatin1Char(' ')));
            cursor = match.capturedEnd();
        }
        rendered += line.mid(cursor);
        if (lineIndex + 1 < lines.size()) {
            rendered += '\n';
        }
    }
    return rendered;
}

QString codeBlockHtml(const QString &code)
{
    QString escaped = formatCodeBlockForDisplay(code).toHtmlEscaped();
    escaped.replace(' ', "&nbsp;");
    return escaped;
}

QString codeBlockPopupHtml(const QString &code, const QString &type = QString())
{
    const QString typeLine = type.isEmpty()
                                 ? QString()
                                 : QString("Type: %1<br>").arg(type.toHtmlEscaped());
    return QString("%1<pre>%2</pre>").arg(typeLine, codeBlockHtml(code));
}

constexpr const char *codeBlockMimeType = "application/x-compile-spire-code-block";

class CodeBlockIcon : public QToolButton
{
public:
    explicit CodeBlockIcon(const QString &blockId, const QString &iconPath, QWidget *parent = nullptr)
        : QToolButton(parent)
        , m_blockId(blockId)
    {
        setFixedSize(74, 74);
        setCursor(Qt::OpenHandCursor);
        setToolButtonStyle(Qt::ToolButtonIconOnly);
        setIcon(QIcon(iconPath));
        setIconSize(QSize(62, 62));
        setStyleSheet(
            "QToolButton { background: rgba(7, 13, 19, 185); border: 2px solid #d7b75f; border-radius: 8px; }"
            "QToolButton:hover { background: rgba(18, 45, 55, 220); border-color: #49e6ff; }"
        );
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) {
            QToolButton::mousePressEvent(event);
            return;
        }

        QDrag *drag = new QDrag(this);
        QMimeData *mime = new QMimeData();
        mime->setData(codeBlockMimeType, m_blockId.toUtf8());
        mime->setText(m_blockId);
        drag->setMimeData(mime);

        QPixmap pixmap = icon().pixmap(iconSize());
        drag->setPixmap(pixmap);
        drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));
        drag->exec(Qt::CopyAction);
    }

private:
    QString m_blockId;
};

class CodeDropSlot : public QLabel
{
public:
    explicit CodeDropSlot(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setAcceptDrops(true);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        setMinimumSize(132, 32);
        setProperty("blockId", QString());
        setText("              ");
        setTextFormat(Qt::PlainText);
        refreshStyle(false);
    }

    void setOnChanged(std::function<void()> callback)
    {
        m_onChanged = std::move(callback);
    }

    void setTextProvider(std::function<QString(const QString &)> provider)
    {
        m_textProvider = std::move(provider);
    }

    void clearBlock()
    {
        setProperty("blockId", QString());
        setProperty("previousBlockId", QString());
        setText("              ");
        setMinimumWidth(132);
        refreshStyle(false);
    }

    void setBlock(const QString &blockId, const QString &displayText)
    {
        setProperty("blockId", blockId);
        setText(displayText);
        setMinimumWidth(displayText.contains('\n') ? 220 : 132);
        refreshStyle(false);
    }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasFormat(codeBlockMimeType)) {
            event->acceptProposedAction();
            refreshStyle(true);
        }
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        refreshStyle(false);
        QLabel::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        const QString blockId = QString::fromUtf8(event->mimeData()->data(codeBlockMimeType)).trimmed();
        if (blockId.isEmpty()) {
            return;
        }

        setProperty("previousBlockId", property("blockId").toString());
        setBlock(blockId, m_textProvider ? m_textProvider(blockId) : blockId);
        event->acceptProposedAction();
        if (m_onChanged) {
            m_onChanged();
        }
    }

private:
    void refreshStyle(bool hovered)
    {
        const bool filled = !property("blockId").toString().isEmpty();
        const QString border = hovered ? "#49e6ff" : (filled ? "#d7b75f" : "#526170");
        const QString background = filled ? "rgba(10, 38, 47, 225)" : "rgba(12, 18, 26, 215)";
        setStyleSheet(QString(
            "QLabel { color: %1; background: %2; border: 2px solid %3; border-radius: 7px;"
            "font-family: Consolas, 'Microsoft YaHei UI'; font-size: 15px; font-weight: 700; padding: 5px 9px; }"
        ).arg(filled ? "#f9f1d0" : "#6f7f8b", background, border));
    }

    std::function<void()> m_onChanged;
    std::function<QString(const QString &)> m_textProvider;
};

class HoverPopupFilter : public QObject
{
public:
    HoverPopupFilter(QWidget *anchor, const QString &html)
        : QObject(anchor)
        , m_anchor(anchor)
        , m_html(html)
    {
        m_popup = new QLabel(nullptr, Qt::ToolTip);
        m_popup->setTextFormat(Qt::RichText);
        m_popup->setWordWrap(true);
        m_popup->setMaximumWidth(360);
        m_popup->setStyleSheet("QLabel { background: rgba(14, 18, 24, 245); color: #f9f1d0; border: 2px solid #49e6ff; border-radius: 6px; padding: 10px; }");
    }

    ~HoverPopupFilter() override
    {
        delete m_popup;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched != m_anchor || !m_popup) {
            return QObject::eventFilter(watched, event);
        }

        if (event->type() == QEvent::Enter) {
            m_popup->setText(m_html);
            m_popup->adjustSize();
            m_popup->move(m_anchor->mapToGlobal(QPoint(m_anchor->width() + 10, 0)));
            m_popup->show();
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::MouseButtonPress || event->type() == QEvent::Hide) {
            m_popup->hide();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QWidget *m_anchor = nullptr;
    QLabel *m_popup = nullptr;
    QString m_html;
};

void installHoverPopup(QWidget *widget, const QString &html)
{
    widget->installEventFilter(new HoverPopupFilter(widget, html));
    widget->setMouseTracking(true);
}

LevelData createPreviewLevel()
{
    LevelData level;
    level.mapWidth = 7;
    level.mapHeight = 6;
    level.bagSize = 5;
    level.levelType = "preview";
    level.startpos = QPoint(1, 1);
    level.endText = "Preview boss cleared.";
    level.specialTags = {"preview_map"};
    level.mapGrid = {
        {"#", "#", "#", "#", "#", "#", "#"},
        {"#", "start", ".", "chest_1", ".", "monster_1", "#"},
        {"#", ".", "#", ".", "#", ".", "#"},
        {"#", "clue_1", ".", ".", ".", "monster_2", "#"},
        {"#", ".", "chest_2", "#", ".", "boss", "#"},
        {"#", "#", "#", "#", "#", "#", "#"}
    };

    Clue clue;
    clue.pos = QPoint(3, 1);
    clue.val = "cout << *func1(x);";
    level.clues["clue_1"] = clue;

    Chest chest1;
    chest1.pos = QPoint(1, 3);
    chest1.chestId = "chest_1";
    chest1.forcedPick = true;
    chest1.repeat = true;
    chest1.blocks["block_add1"] = CodeBlock{"block_add1", "a += 1;"};
    chest1.blocks["block_mul2"] = CodeBlock{"block_mul2", "a *= 2;"};
    level.chests[chest1.chestId] = chest1;

    Chest chest2;
    chest2.pos = QPoint(4, 2);
    chest2.chestId = "chest_2";
    chest2.forcedPick = false;
    chest2.repeat = false;
    chest2.blocks["block_ret_val"] = CodeBlock{"block_ret_val", "return a;"};
    chest2.blocks["block_ret_loc"] = CodeBlock{"block_ret_loc", "return &a;"};
    level.chests[chest2.chestId] = chest2;

    Monster monster1;
    monster1.pos = QPoint(1, 5);
    monster1.monsterId = "monster_1";
    monster1.name = "func_ret";
    monster1.nickname = "Runtime Bug";
    monster1.type = "function";
    monster1.codeTemplate = "int* func1(int& a){\n    a++;\n    $space1$\n}";
    monster1.spaces.append(Space{"space1", "regex", {"^block.*"}});
    level.monsters[monster1.monsterId] = monster1;

    Monster monster2;
    monster2.pos = QPoint(3, 5);
    monster2.monsterId = "monster_2";
    monster2.name = "class_calc";
    monster2.nickname = "Stack Trace";
    monster2.type = "class";
    monster2.codeTemplate = "class A{\npublic:\n    int x;\n    A(int a=0){\n        $space1$\n        $space2$\n        x=a;\n    }\n};";
    monster2.spaces.append(Space{"space1", "prefix", {"block"}});
    monster2.spaces.append(Space{"space2", "find", {"block"}});
    level.monsters[monster2.monsterId] = monster2;

    Boss boss;
    boss.pos = QPoint(4, 5);
    boss.monsterId = "boss";
    boss.name = "boss";
    boss.nickname = "Marry Ann";
    boss.type = "boss";
    boss.codeTemplate = "$space1$\n$space2$\nint main(){ return 0; }";
    boss.input = "1";
    boss.output = "5";
    boss.spaces.append(Space{"space1", "match", {"func_ret[block_ret_loc]"}});
    boss.spaces.append(Space{"space2", "match", {"class_calc[block_add1,block_mul2]"}});
    level.boss = boss;

    return level;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resize(1366, 768);
    applyVisualStyle();
    applyDefaultSettings();
    buildRuntimeGameUi();
    setupMovementShortcuts();
    bgmAudioOutput = new QAudioOutput(this);
    bgmPlayer = new QMediaPlayer(this);
    bgmPlayer->setAudioOutput(bgmAudioOutput);
    updateBgmVolume();
    connect(bgmPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia && !currentBgmResource.isEmpty()) {
            bgmPlayer->setPosition(0);
            bgmPlayer->play();
        }
    });
    connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, [this]() {
        syncBgmToCurrentPage();
    });
    connect(&gameEngine, &GameEngine::levelLoaded, this, [this]() {
        syncFromEngineState();
        ui->combatLogLabel->setText("Use WASD/arrow keys or click a reachable tile.");
        refreshGameUi();
    });
    connect(&gameEngine, &GameEngine::moveCompleted, this, [this](const MoveResult &result) {
        activeMovePath.clear();
        if (!suppressNextMovePath) {
            for (const QPoint &step : result.movePath) {
                activeMovePath.append(QPoint(step.y(), step.x()));
            }
            activeMovePathIndex = activeMovePath.size();
        }
        suppressNextMovePath = false;
        syncFromEngineState();
        ui->combatLogLabel->setText(result.event == "empty"
                                        ? QString("Moved to %1,%2.").arg(playerColumn).arg(playerRow)
                                        : QString("Triggered %1: %2").arg(result.event, result.eventId));
        refreshGameUi();
        if (result.event == "clue") {
            QTimer::singleShot(0, this, [this, clueId = result.eventId]() {
                if (gameEngine.m_map && !gameEngine.m_map->clueRevealed(clueId)) {
                    const bool revealed = gameEngine.revealClue(clueId);
                    syncFromEngineState();
                    refreshGameUi();
                    if (revealed && currentLevelIndex >= 0 && currentLevelIndex < levels.size()) {
                        const QString clueText = levels.at(currentLevelIndex).clues.value(clueId).val.trimmed();
                        QMessageBox::information(this,
                                                 "Clue",
                                                 clueText.isEmpty() ? QString("%1 recorded.").arg(clueId) : clueText);
                    }
                }
            });
        }
    });
    connect(&gameEngine, &GameEngine::forcedMove, this, [this](const QPoint &) {
        activeMovePath.clear();
        activeMovePathIndex = 0;
        movePlaybackActive = false;
        suppressNextMovePath = false;
        syncFromEngineState();
        refreshGameUi();
    });
    connect(&gameEngine, &GameEngine::chestEntered, this, [this](const QString &chestId) {
        QTimer::singleShot(0, this, [this, chestId]() {
            handleChest(chestId);
            syncFromEngineState();
            refreshGameUi();
        });
    });
    connect(&gameEngine, &GameEngine::combatStarted, this, [this](const QString &monsterId, Combat *) {
        QTimer::singleShot(0, this, [this, monsterId]() {
            if (monsterId != "boss" && !monsterId.startsWith("monster")) {
                if (gameEngine.m_combat) {
                    gameEngine.exitCombat();
                }
                syncFromEngineState();
                refreshGameUi();
                ui->combatLogLabel->setText("Ignored a stale cleared event tile.");
                return;
            }
            if (!gameEngine.m_combat) {
                syncFromEngineState();
                refreshGameUi();
                ui->combatLogLabel->setText("Combat event was ignored because no backend combat is active.");
                return;
            }
            handleMonster(monsterId);
            syncFromEngineState();
            refreshGameUi();
        });
    });
    connect(&gameEngine, &GameEngine::combatEnded, this, [this](const CombatResult &) {
        syncFromEngineState();
        refreshGameUi();
    });
    connect(&gameEngine, &GameEngine::levelUnlocked, this, [this](int levelIndex) {
        completedStageIndexes.insert(levelIndex - 1);
        refreshLevelSelectUi();
    });
    loadLevels();
    refreshLevelSelectUi();
    ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
    mainMenuButtonBar->show();
    mainMenuButtonBar->raise();
    syncBgmToCurrentPage();
    QTimer::singleShot(0, this, &MainWindow::positionMainMenuButtons);

    connect(ui->startGameButton, &QPushButton::clicked, this, [this]() {
        mainMenuButtonBar->hide();
        refreshLevelSelectUi();
        ui->stackedWidget->setCurrentWidget(ui->mapPage);
    });

    connect(ui->settingsButton, &QPushButton::clicked, this, [this]() {
        mainMenuButtonBar->hide();
        ui->stackedWidget->setCurrentWidget(ui->settingsPage);
    });

    connect(ui->pauseButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pausePage);
    });

    connect(ui->resumeButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->gamePage);
    });

    connect(ui->pauseSettingsButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->settingsPage);
    });

    connect(ui->pauseMainMenuButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
        mainMenuButtonBar->show();
        positionMainMenuButtons();
    });

    connect(ui->backToMenuButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
        mainMenuButtonBar->show();
        positionMainMenuButtons();
    });

    connect(ui->deckButton, &QPushButton::clicked, this, [this]() {
        clearDisplayedMovePath();
        showBagDialog();
    });

    connect(ui->mapBackButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
        mainMenuButtonBar->show();
        positionMainMenuButtons();
    });

    connect(ui->mapFightButton, &QPushButton::clicked, this, [this]() {
        selectStage(ui->mapFightButton->property("levelIndex").toInt());
    });

    connect(ui->mapEliteButton, &QPushButton::clicked, this, [this]() {
        selectStage(ui->mapEliteButton->property("levelIndex").toInt());
    });

    connect(ui->mapBossButton, &QPushButton::clicked, this, [this]() {
        selectStage(ui->mapBossButton->property("levelIndex").toInt());
    });

    connect(ui->nextChallengeButton, &QPushButton::clicked, this, [this]() {
        undo();
    });

    connect(resetRunButton, &QPushButton::clicked, this, [this]() {
        resetLevel();
    });

    connect(ui->manualButton, &QPushButton::clicked, this, [this]() {
        clearDisplayedMovePath();
        showManualDialog();
    });

    connect(ui->saveSettingsButton, &QPushButton::clicked, this, [this]() {
        if (ui->windowModeComboBox->currentText() == "Fullscreen" || ui->fullscreenCheckBox->isChecked()) {
            showFullScreen();
        } else {
            showNormal();
        }
        statusBar()->showMessage("Settings applied.", 2500);
    });

    connect(ui->resetSettingsButton, &QPushButton::clicked, this, [this]() {
        applyDefaultSettings();
        statusBar()->showMessage("Settings restored to defaults.", 2500);
    });

    const auto updatePercent = [](QSlider *slider, QLabel *label) {
        label->setText(QString("%1%").arg(slider->value()));
    };

    connect(ui->volumeSlider, &QSlider::valueChanged, this, [this, updatePercent]() {
        updatePercent(ui->volumeSlider, ui->volumeValueLabel);
        updateBgmVolume();
    });
    connect(ui->musicVolumeSlider, &QSlider::valueChanged, this, [this, updatePercent]() {
        updatePercent(ui->musicVolumeSlider, ui->musicVolumeValueLabel);
        updateBgmVolume();
    });
    connect(ui->sfxVolumeSlider, &QSlider::valueChanged, this, [this, updatePercent]() {
        updatePercent(ui->sfxVolumeSlider, ui->sfxVolumeValueLabel);
    });

    connect(ui->exitButton, &QPushButton::clicked, this, &MainWindow::close);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupMovementShortcuts()
{
    qApp->installEventFilter(this);
    setFocusPolicy(Qt::StrongFocus);
    if (mapView) {
        mapView->setFocusPolicy(Qt::StrongFocus);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress
        && ui
        && ui->stackedWidget->currentWidget() == ui->gamePage
        && !QApplication::activeModalWidget()) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (handleGameMoveKey(keyEvent->key())) {
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::handleGameMoveKey(int key)
{
    if (ui->stackedWidget->currentWidget() == ui->gamePage) {
        switch (key) {
        case Qt::Key_W:
        case Qt::Key_Up:
            movePlayer(-1, 0);
            return true;
        case Qt::Key_S:
        case Qt::Key_Down:
            movePlayer(1, 0);
            return true;
        case Qt::Key_A:
        case Qt::Key_Left:
            movePlayer(0, -1);
            return true;
        case Qt::Key_D:
        case Qt::Key_Right:
            movePlayer(0, 1);
            return true;
        default:
            break;
        }
    }
    return false;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (!QApplication::activeModalWidget() && handleGameMoveKey(event->key())) {
        return;
    }
    if (ui->stackedWidget->currentWidget() == ui->gamePage
        && event->key() == Qt::Key_Z
        && (event->modifiers() & Qt::ControlModifier)) {
        undo();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    positionMainMenuButtons();
}

void MainWindow::applyDefaultSettings()
{
    ui->volumeSlider->setValue(80);
    ui->musicVolumeSlider->setValue(70);
    ui->sfxVolumeSlider->setValue(75);
    ui->windowModeComboBox->setCurrentIndex(0);
    ui->resolutionComboBox->setCurrentIndex(0);
    ui->fullscreenCheckBox->setChecked(false);
    ui->languageComboBox->setCurrentIndex(0);
    ui->animationSpeedComboBox->setCurrentIndex(0);
    ui->tutorialCheckBox->setChecked(true);
    ui->volumeValueLabel->setText("80%");
    ui->musicVolumeValueLabel->setText("70%");
    ui->sfxVolumeValueLabel->setText("75%");
}

void MainWindow::applyVisualStyle()
{
    setStyleSheet(R"(
        QMainWindow, QWidget {
            background: #121316;
            color: #f1ead8;
            font-family: "Microsoft YaHei UI";
            font-size: 14px;
        }

        QLabel#titleLabel {
            color: #f6d878;
            font-size: 34px;
            font-weight: 700;
        }

        QWidget#mainMenuPage {
            border-image: url(:/images/assets/title_cover_16_9.png) 0 0 0 0 stretch stretch;
        }

        QWidget#mapPage {
            border-image: url(:/images/assets/stage_select_bg.png) 0 0 0 0 stretch stretch;
        }

        QWidget#settingsPage {
            background-color: #15161a;
            border-image: url(:/images/assets/interface_background_fitted.png) 0 0 0 0 stretch stretch;
        }

        QPushButton#startGameButton, QPushButton#settingsButton, QPushButton#exitButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(58, 38, 14, 225),
                                        stop:0.48 rgba(20, 24, 30, 225),
                                        stop:1 rgba(5, 8, 13, 238));
            color: #fff0b7;
            border: 3px solid #d59731;
            border-radius: 8px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 20px;
            font-weight: 900;
            padding: 8px 24px;
            min-width: 220px;
            max-width: 220px;
            min-height: 52px;
            max-height: 52px;
            letter-spacing: 0px;
        }

        QPushButton#startGameButton {
            border-color: #ffd25b;
            color: #fff6c8;
        }

        QPushButton#startGameButton:hover, QPushButton#settingsButton:hover, QPushButton#exitButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(55, 91, 94, 235),
                                        stop:0.5 rgba(16, 39, 48, 238),
                                        stop:1 rgba(7, 10, 16, 245));
            border-color: #57edf6;
            color: #ffffff;
        }

        QPushButton#startGameButton:pressed, QPushButton#settingsButton:pressed, QPushButton#exitButton:pressed {
            background: rgba(7, 9, 14, 245);
            border-color: #ff8f3e;
            color: #ffdca4;
            padding-top: 11px;
            padding-bottom: 5px;
        }

        QLabel#mapTitleLabel {
            color: #f6d878;
            font-size: 24px;
            font-weight: 700;
        }

        QLabel#menuSubtitleLabel, QLabel#settingsSubtitleLabel, QLabel#combatLogLabel, QLabel#tileInfoLabel {
            color: #cfc6ad;
        }

        QFrame, QGroupBox, QListWidget {
            background: #242832;
            border: 1px solid #454b59;
            border-radius: 6px;
        }

        QLabel#settingsTitleLabel {
            color: #f1d37c;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 26px;
            font-weight: 900;
            padding: 6px 22px;
            background: rgba(27, 20, 12, 220);
            border: 2px solid #d7b75f;
            border-radius: 6px;
        }

        QLabel#settingsTitleLabel {
            max-width: 420px;
        }

        QLabel#settingsSubtitleLabel {
            background: rgba(24, 24, 26, 170);
            border: 1px solid rgba(90, 90, 90, 110);
            border-radius: 4px;
            padding: 4px 12px;
        }

        QFrame[bagPanel="true"] {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(58, 43, 25, 245),
                                        stop:0.55 rgba(28, 22, 15, 245),
                                        stop:1 rgba(16, 13, 9, 248));
            border: 3px solid #090706;
            border-radius: 6px;
        }

        QFrame[bagBoard="true"] {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(48, 39, 26, 242),
                                        stop:1 rgba(20, 17, 12, 248));
            border: 4px solid #090706;
            border-radius: 4px;
        }

        QLabel[bagSection="true"] {
            color: #f2d790;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 17px;
            font-weight: 900;
            background: transparent;
            border: none;
        }

        QLabel[codeStrip="true"] {
            color: #1c1208;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 #fff0ca,
                                        stop:0.50 #d49d61,
                                        stop:1 #7a4a25);
            border: 2px solid #100b08;
            border-radius: 4px;
            padding: 6px 8px;
            font-family: "Consolas", "Cascadia Mono", "Microsoft YaHei UI";
            font-size: 13px;
            font-weight: 700;
        }

        QPushButton[bagPageButton="true"] {
            background: rgba(24, 18, 12, 230);
            border: 2px solid #d0aa59;
            border-radius: 5px;
            color: #ffe5a0;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-weight: 900;
        }

        QFrame#mapFrame {
            background: transparent;
            border: none;
            border-radius: 0px;
        }

        QFrame#combatFrame {
            background: #191d24;
            border: 2px solid #3d4652;
            border-radius: 6px;
        }

        QFrame#challengeFrame, QFrame#bottomBarFrame, QFrame#topBarFrame {
            background: #20242c;
            border: 1px solid #515968;
        }

        QGroupBox {
            margin-top: 12px;
            padding-top: 14px;
            font-weight: 700;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: #f4d58d;
        }

        QPushButton {
            background: #344151;
            border: 1px solid #708197;
            border-radius: 5px;
            padding: 7px 14px;
            color: #f8fafc;
        }

        QPushButton:hover {
            background: #3f516b;
        }

        QPushButton:pressed {
            background: #263244;
        }

        QPushButton:disabled {
            background: #24272d;
            color: #777c83;
            border-color: #363b44;
        }

        QPushButton[levelCard="true"] {
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.72,
                                        fx:0.38, fy:0.30,
                                        stop:0 rgba(140, 255, 255, 245),
                                        stop:0.18 rgba(40, 183, 212, 238),
                                        stop:0.54 rgba(8, 45, 64, 245),
                                        stop:1 rgba(1, 8, 14, 255));
            color: #eaffff;
            border: 4px solid rgba(80, 240, 255, 225);
            border-radius: 42px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 30px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelCard="true"]:hover {
            background: qradialgradient(cx:0.5, cy:0.45, radius:0.75,
                                        stop:0 rgba(255, 255, 255, 245),
                                        stop:0.28 rgba(91, 242, 255, 245),
                                        stop:0.68 rgba(12, 68, 90, 250),
                                        stop:1 rgba(3, 8, 14, 255));
            border-color: #c6fbff;
            color: #ffffff;
        }

        QPushButton[levelCard="selected"] {
            background: qradialgradient(cx:0.5, cy:0.43, radius:0.75,
                                        stop:0 rgba(255, 252, 188, 255),
                                        stop:0.28 rgba(71, 238, 255, 255),
                                        stop:0.62 rgba(14, 111, 143, 250),
                                        stop:1 rgba(2, 15, 24, 255));
            color: #ffffff;
            border: 5px solid #f7d75b;
            border-radius: 42px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 34px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelCard="cleared"] {
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.74,
                                        stop:0 rgba(255, 244, 166, 250),
                                        stop:0.34 rgba(194, 130, 28, 245),
                                        stop:0.68 rgba(66, 42, 14, 250),
                                        stop:1 rgba(8, 9, 12, 255));
            color: #ffe89b;
            border: 4px solid #ffca55;
            border-radius: 42px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 30px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelCard="locked"] {
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.7,
                                        stop:0 rgba(67, 69, 72, 230),
                                        stop:0.56 rgba(27, 29, 33, 248),
                                        stop:1 rgba(4, 5, 7, 255));
            color: #b6aa92;
            border: 4px solid #5b5144;
            border-radius: 42px;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 28px;
            font-weight: 900;
            padding: 4px;
            text-align: center;
        }

        QPushButton[levelNav="true"], QPushButton#levelStartButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(16, 42, 53, 235),
                                        stop:0.5 rgba(7, 18, 27, 238),
                                        stop:1 rgba(3, 8, 14, 245));
            border: 3px solid #1fd5ec;
            border-radius: 8px;
            color: #65f5ff;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 20px;
            font-weight: 900;
            padding: 7px 18px;
        }

        QPushButton[levelMode="true"] {
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.72,
                                        stop:0 rgba(88, 238, 255, 230),
                                        stop:0.38 rgba(14, 67, 86, 245),
                                        stop:1 rgba(2, 8, 14, 255));
            border: 3px solid #25d9ef;
            border-radius: 18px;
            color: #78f6ff;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 18px;
            font-weight: 900;
            padding: 6px 18px;
        }

        QPushButton[levelNav="true"]:hover, QPushButton[levelMode="true"]:hover, QPushButton#levelStartButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(47, 91, 96, 245),
                                        stop:1 rgba(8, 25, 35, 250));
            border-color: #c2fbff;
            color: #ffffff;
        }

        QPushButton[levelNav="true"]:pressed, QPushButton[levelMode="true"]:pressed, QPushButton#levelStartButton:pressed {
            background: rgba(5, 10, 16, 250);
            border-color: #ffba52;
            color: #ffe9a8;
        }

        QPushButton#levelStartButton {
            font-size: 34px;
            letter-spacing: 0px;
        }

        QPushButton[levelMode="active"] {
            background: qradialgradient(cx:0.5, cy:0.42, radius:0.73,
                                        stop:0 rgba(255, 230, 255, 245),
                                        stop:0.35 rgba(166, 73, 190, 248),
                                        stop:1 rgba(29, 10, 42, 255));
            border: 3px solid #f0a2ff;
            border-radius: 18px;
            color: #fff2ff;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 18px;
            font-weight: 900;
            padding: 6px 18px;
        }

        QLabel#levelPageLabel {
            background: rgba(16, 22, 32, 218);
            border: 2px solid rgba(213, 151, 49, 210);
            border-radius: 8px;
            color: #62f3ff;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 24px;
            font-weight: 900;
            padding: 8px 20px;
            letter-spacing: 0px;
        }

        QLabel[levelBridge="true"] {
            color: rgba(62, 232, 255, 245);
            background: transparent;
            border: none;
            font-size: 42px;
            font-weight: 900;
        }

        QLabel[levelBridge="locked"] {
            color: rgba(115, 128, 139, 210);
            background: transparent;
            border: none;
            font-size: 42px;
            font-weight: 900;
        }

        QLabel[levelBridge="cleared"] {
            color: rgba(255, 206, 91, 235);
            background: transparent;
            border: none;
            font-size: 42px;
            font-weight: 900;
        }

        QLabel[levelPath="true"] {
            color: #f6d878;
            font-size: 28px;
            font-weight: 800;
        }

        QPushButton[tileRole="player"] {
            background: #f6d878;
            color: #15181d;
            border-color: #fff4c2;
            font-weight: 700;
        }

        QPushButton[tileRole="monster"] {
            background: #842626;
            border-color: #ff776d;
            color: #fff1eb;
        }

        QPushButton[tileRole="chest"] {
            background: #946318;
            border-color: #ffd15a;
            color: #fff3c2;
        }

        QPushButton[tileRole="clue"] {
            background: #1e6550;
            border-color: #45d39f;
            color: #daf7eb;
        }

        QPushButton[tileRole="boss"] {
            background: #5f2d7d;
            border-color: #d99cff;
            color: #f5e8ff;
        }

        QPushButton[tileRole="wall"] {
            background: #30343b;
            border-color: #171a1f;
            color: #737987;
        }

        QPushButton[tileRole="floor"] {
            background: #323947;
            border-color: #576172;
            color: #aeb7c8;
        }

        QPushButton[tileRole="start"] {
            background: #245f3d;
            border-color: #6de09c;
            color: #dfffe8;
        }

        QPushButton[tileRole="cleared"] {
            background: #2d3641;
            border-color: #596474;
            color: #97a3b5;
        }

        QPushButton[mapTile="true"] {
            border-width: 2px;
            border-radius: 4px;
            font-size: 13px;
            font-weight: 700;
            padding: 4px;
        }

        QTextEdit, QLineEdit {
            background: #111827;
            border: 1px solid #3d4652;
            border-radius: 5px;
            color: #f9f1d0;
            padding: 8px;
        }

        QComboBox, QSlider, QLineEdit {
            min-height: 28px;
        }

        QListWidget {
            padding: 10px;
        }
    )");
}

void MainWindow::buildRuntimeGameUi()
{
    if (QLayout *oldMenuLayout = ui->mainMenuPage->layout()) {
        delete oldMenuLayout;
    }
    ui->titleLabel->hide();
    ui->menuSubtitleLabel->hide();
    ui->mainMenuBackgroundLabel->hide();
    mainMenuButtonBar = new QWidget(ui->mainMenuPage);
    mainMenuButtonBar->setObjectName("mainMenuButtonBar");
    mainMenuButtonBar->setStyleSheet("QWidget#mainMenuButtonBar { background: transparent; border: none; }");
    QHBoxLayout *menuButtonRow = new QHBoxLayout(mainMenuButtonBar);
    menuButtonRow->setSpacing(40);
    menuButtonRow->setContentsMargins(0, 0, 0, 0);
    ui->startGameButton->setParent(mainMenuButtonBar);
    ui->settingsButton->setParent(mainMenuButtonBar);
    ui->exitButton->setParent(mainMenuButtonBar);
    menuButtonRow->addWidget(ui->startGameButton);
    menuButtonRow->addWidget(ui->settingsButton);
    menuButtonRow->addWidget(ui->exitButton);
    ui->startGameButton->setFixedSize(220, 52);
    ui->settingsButton->setFixedSize(220, 52);
    ui->exitButton->setFixedSize(220, 52);
    mainMenuButtonBar->show();
    mainMenuButtonBar->raise();
    positionMainMenuButtons();
    QTimer::singleShot(0, this, &MainWindow::positionMainMenuButtons);

    QList<QLabel *> settingsLabels = {
        ui->musicVolumeLabel,
        ui->sfxVolumeLabel,
        ui->windowModeLabel,
        ui->resolutionLabel,
        ui->languageLabel,
        ui->animationSpeedLabel
    };
    if (QLabel *volumeLabel = findChild<QLabel *>("volumeLabel")) {
        settingsLabels.prepend(volumeLabel);
    }
    for (QLabel *label : settingsLabels) {
        if (!label) {
            continue;
        }
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setFixedWidth(150);
    }

    ui->hpLabel->setText("Mode: Explore");
    ui->deckButton->setText("Bag");
    ui->manualButton->setText("Manual");
    ui->challengeTitleLabel->setText("Interaction");
    ui->answerLabel->setText("Blocks");
    ui->answerLineEdit->setPlaceholderText("Example: block_add1, block_mul2");
    ui->submitAnswerButton->setText("Fill");
    ui->challengeTextEdit->setContextMenuPolicy(Qt::NoContextMenu);
    ui->challengeTextEdit->setTextInteractionFlags(Qt::NoTextInteraction);
    ui->challengeFrame->hide();
    ui->mapButton->hide();
    ui->combatLogLabel->show();
    ui->nextChallengeButton->setText("Undo");
    ui->nextChallengeButton->show();
    ui->nextChallengeButton->setEnabled(false);
    ui->bottomBarLayout->setStretch(0, 0);
    ui->bottomBarLayout->setStretch(1, 0);
    ui->bottomBarLayout->setStretch(2, 0);
    ui->bottomBarLayout->setStretch(3, 1);
    ui->bottomBarLayout->setStretch(4, 0);

    ui->mapTitleLabel->hide();
    ui->mapLayout->setContentsMargins(28, 22, 28, 20);
    ui->mapLayout->setSpacing(4);
    ui->mapFrame->setMinimumHeight(520);
    ui->mapBackButton->setProperty("levelNav", "true");
    ui->mapBackButton->setMinimumSize(190, 52);
    ui->mapFightButton->setProperty("levelCard", "true");
    ui->mapEliteButton->setProperty("levelCard", "true");
    ui->mapBossButton->setProperty("levelCard", "true");
    while (QLayoutItem *item = ui->mapNodeLayout->takeAt(0)) {
        delete item;
    }
    ui->mapNodeLayout->setContentsMargins(92, 24, 92, 18);
    ui->mapNodeLayout->setSpacing(8);

    QHBoxLayout *modeLayout = new QHBoxLayout();
    modeLayout->setSpacing(150);
    QPushButton *normalButton = new QPushButton("NORMAL", ui->mapFrame);
    QPushButton *exButton = new QPushButton("EX", ui->mapFrame);
    normalButton->setFixedSize(132, 58);
    exButton->setFixedSize(132, 58);
    normalButton->setProperty("levelMode", "active");
    exButton->setProperty("levelMode", "true");
    connect(normalButton, &QPushButton::clicked, this, [this]() {
        setLevelSelectMode(false);
    });
    connect(exButton, &QPushButton::clicked, this, [this]() {
        setLevelSelectMode(true);
    });
    normalButton->setObjectName("normalStageButton");
    exButton->setObjectName("exStageButton");
    modeLayout->addStretch();
    modeLayout->addWidget(normalButton);
    modeLayout->addWidget(exButton);
    modeLayout->addStretch();
    ui->mapNodeLayout->addLayout(modeLayout);

    QLabel *pageLabel = new QLabel("SELECT STAGE", ui->mapFrame);
    pageLabel->setObjectName("levelPageLabel");
    pageLabel->setAlignment(Qt::AlignCenter);
    pageLabel->setFixedHeight(48);
    ui->mapNodeLayout->addWidget(pageLabel);

    QHBoxLayout *stageLayout = new QHBoxLayout();
    stageLayout->setSpacing(38);
    QPushButton *prevButton = new QPushButton("<", ui->mapFrame);
    QPushButton *nextButton = new QPushButton(">", ui->mapFrame);
    prevButton->setFixedSize(70, 70);
    nextButton->setFixedSize(70, 70);
    prevButton->setProperty("levelNav", "true");
    nextButton->setProperty("levelNav", "true");
    connect(prevButton, &QPushButton::clicked, this, [this]() {
        changeLevelPage(-1);
    });
    connect(nextButton, &QPushButton::clicked, this, [this]() {
        changeLevelPage(1);
    });
    prevButton->setObjectName("prevStageButton");
    nextButton->setObjectName("nextStageButton");
    ui->mapFightButton->setObjectName("stageNodeButton0");
    ui->mapEliteButton->setObjectName("stageNodeButton1");
    ui->mapBossButton->setObjectName("stageNodeButton2");
    ui->mapFightButton->setFixedSize(84, 84);
    ui->mapEliteButton->setFixedSize(84, 84);
    ui->mapBossButton->setFixedSize(84, 84);

    QWidget *nodeCanvas = new QWidget(ui->mapFrame);
    nodeCanvas->setObjectName("stageNodeCanvas");
    nodeCanvas->setFixedSize(650, 250);
    nodeCanvas->setStyleSheet("QWidget#stageNodeCanvas { background: transparent; border: none; }");
    QLabel *bridge01 = new QLabel(nodeCanvas);
    QLabel *bridge12 = new QLabel(nodeCanvas);
    bridge01->setObjectName("stageBridge0");
    bridge12->setObjectName("stageBridge1");
    bridge01->setProperty("levelBridge", "true");
    bridge12->setProperty("levelBridge", "true");
    bridge01->setAlignment(Qt::AlignCenter);
    bridge12->setAlignment(Qt::AlignCenter);
    bridge01->setPixmap(QPixmap(":/images/assets/stage_bridge_up.png"));
    bridge12->setPixmap(QPixmap(":/images/assets/stage_bridge_up2.png"));
    bridge01->setScaledContents(true);
    bridge12->setScaledContents(true);
    bridge01->setGeometry(158, 112, 250, 108);
    bridge12->setGeometry(366, 38, 250, 108);
    ui->mapFightButton->setParent(nodeCanvas);
    ui->mapEliteButton->setParent(nodeCanvas);
    ui->mapBossButton->setParent(nodeCanvas);
    ui->mapFightButton->setGeometry(92, 160, 84, 84);
    ui->mapEliteButton->setGeometry(300, 92, 84, 84);
    ui->mapBossButton->setGeometry(512, 18, 84, 84);
    bridge01->lower();
    bridge12->lower();
    bridge01->setStyleSheet("background: transparent; border: none;");
    bridge12->setStyleSheet("background: transparent; border: none;");
    ui->mapFightButton->show();
    ui->mapEliteButton->show();
    ui->mapBossButton->show();
    stageLayout->addWidget(prevButton, 0, Qt::AlignVCenter);
    stageLayout->addStretch(1);
    stageLayout->addWidget(nodeCanvas, 0, Qt::AlignCenter);
    stageLayout->addStretch(1);
    stageLayout->addWidget(nextButton, 0, Qt::AlignVCenter);
    ui->mapNodeLayout->addLayout(stageLayout, 1);

    QPushButton *levelStartButton = new QPushButton("START", ui->mapFrame);
    levelStartButton->setObjectName("levelStartButton");
    levelStartButton->setFixedSize(280, 50);
    connect(levelStartButton, &QPushButton::clicked, this, [this]() {
        startLevel(selectedStageIndex);
    });
    ui->mapNodeLayout->addSpacing(4);
    ui->mapNodeLayout->addWidget(levelStartButton, 0, Qt::AlignCenter);

    clearLayout(ui->combatLayout);
    mapView = new MapView(ui->combatFrame);
    connect(mapView, &MapView::tileClicked, this, [this](int row, int column) {
        movePlayerTo(row, column);
    });
    ui->combatLayout->addWidget(mapView, 1);

    QFrame *sideFrame = new QFrame(ui->combatFrame);
    sideFrame->setObjectName("sideFrame");
    QVBoxLayout *sideLayout = new QVBoxLayout(sideFrame);
    QLabel *sideTitle = new QLabel("Tile Info", sideFrame);
    QFont sideFont = sideTitle->font();
    sideFont.setPointSize(16);
    sideFont.setBold(true);
    sideTitle->setFont(sideFont);
    tileInfoLabel = new QLabel("Choose a level to enter the map.", sideFrame);
    tileInfoLabel->setObjectName("tileInfoLabel");
    tileInfoLabel->setWordWrap(true);
    sideLayout->addWidget(sideTitle);
    sideLayout->addWidget(tileInfoLabel, 1);
    sideFrame->setMaximumWidth(220);
    ui->combatLayout->addWidget(sideFrame, 0);

    resetRunButton = new QPushButton("Reset", ui->topBarFrame);
    ui->topBarLayout->insertWidget(ui->topBarLayout->count() - 1, resetRunButton);

}

void MainWindow::positionMainMenuButtons()
{
    if (!mainMenuButtonBar) {
        return;
    }

    const int buttonWidth = 220;
    const int gap = 40;
    const int width = 3 * buttonWidth + 2 * gap;
    const int height = 60;
    const int x = qMax(0, (ui->mainMenuPage->width() - width) / 2);
    const int y = qMax(0, ui->mainMenuPage->height() - height - 6);
    mainMenuButtonBar->setGeometry(x, y, width, height);
    mainMenuButtonBar->raise();
}

void MainWindow::updateMainMenuBackground()
{
}

void MainWindow::playBgm(const QString &resourcePath)
{
    if (!bgmPlayer || resourcePath.isEmpty() || currentBgmResource == resourcePath) {
        return;
    }

    const QString appRelativePath = QDir(QCoreApplication::applicationDirPath()).filePath(resourcePath);
    const QString workingRelativePath = QDir::current().filePath(resourcePath);
    QString audioPath = QFileInfo::exists(appRelativePath) ? appRelativePath : workingRelativePath;
    if (!QFileInfo::exists(audioPath)) {
        audioPath = QDir(QCoreApplication::applicationDirPath()).filePath(QString("../CompileTheSpire/%1").arg(resourcePath));
    }
    if (!QFileInfo::exists(audioPath)) {
        statusBar()->showMessage(QString("Missing BGM: %1").arg(resourcePath), 2500);
        return;
    }

    currentBgmResource = resourcePath;
    bgmPlayer->setSource(QUrl::fromLocalFile(audioPath));
    updateBgmVolume();
    bgmPlayer->play();
}

void MainWindow::syncBgmToCurrentPage()
{
    if (!ui || !ui->stackedWidget) {
        return;
    }

    QWidget *page = ui->stackedWidget->currentWidget();
    if (page == ui->mainMenuPage) {
        playBgm("assets/audio/bgm_menu.mp3");
    } else if (page == ui->mapPage) {
        playBgm("assets/audio/bgm_stage_select.mp3");
    } else if (page == ui->gamePage || page == ui->pausePage) {
        playBgm("assets/audio/bgm_game.mp3");
    } else if (page == ui->settingsPage && currentBgmResource.isEmpty()) {
        playBgm("assets/audio/bgm_menu.mp3");
    }
}

void MainWindow::updateBgmVolume()
{
    if (!bgmAudioOutput || !ui) {
        return;
    }

    const qreal master = qBound(0, ui->volumeSlider->value(), 100) / 100.0;
    const qreal music = qBound(0, ui->musicVolumeSlider->value(), 100) / 100.0;
    bgmAudioOutput->setVolume(master * music);
}

void MainWindow::loadLevels()
{
    QStringList errors;
    for (const QString &path : fallbackLevelPaths()) {
        Q_UNUSED(errors);
        gameEngine.gameInit(path);
        if (!gameEngine.levels.isEmpty()) {
            levels = gameEngine.levels;
            statusBar()->showMessage(QString("Loaded %1 level(s) from %2").arg(levels.size()).arg(path), 3000);
            return;
        }
    }

    LevelData single;
    QString error;
    const QString examplePath = QDir::current().filePath("example.json");
    if (single.LoadFromJson(examplePath, &error)) {
        levels.append(single);
        gameEngine.levels = levels;
        gameEngine.m_save.Init(levels.size());
        gameEngine.m_save.Load(levels.size());
        statusBar()->showMessage("Loaded example.json.", 3000);
        return;
    }

    levels.append(createPreviewLevel());
    gameEngine.levels = levels;
    gameEngine.m_save.Init(levels.size());
    gameEngine.m_save.Load(levels.size());
    statusBar()->showMessage("No valid level JSON was found. Loaded preview map for UI testing.", 5000);
}

void MainWindow::refreshLevelSelectUi()
{
    const QVector<StageCard> allStages = stageCatalog();
    QVector<StageCard> visibleStages;
    for (const StageCard &stage : allStages) {
        if (stage.isEx == showingExLevels) {
            visibleStages.append(stage);
        }
    }

    const int pageCount = qMax(1, (visibleStages.size() + stagesPerPage - 1) / stagesPerPage);
    currentLevelSelectPage = qBound(0, currentLevelSelectPage, pageCount - 1);
    const int firstIndex = currentLevelSelectPage * stagesPerPage;

    ui->mapTitleLabel->setText(showingExLevels ? "EX Stage Select" : "Select Stage");
    QPushButton *buttons[] = {ui->mapFightButton, ui->mapEliteButton, ui->mapBossButton};

    QLabel *pageLabel = ui->mapFrame->findChild<QLabel *>("levelPageLabel");
    QPushButton *prevButton = ui->mapFrame->findChild<QPushButton *>("prevStageButton");
    QPushButton *nextButton = ui->mapFrame->findChild<QPushButton *>("nextStageButton");
    QPushButton *normalButton = ui->mapFrame->findChild<QPushButton *>("normalStageButton");
    QPushButton *exButton = ui->mapFrame->findChild<QPushButton *>("exStageButton");
    QPushButton *startButton = ui->mapFrame->findChild<QPushButton *>("levelStartButton");
    QLabel *bridges[] = {
        ui->mapFrame->findChild<QLabel *>("stageBridge0"),
        ui->mapFrame->findChild<QLabel *>("stageBridge1")
    };

    if (pageLabel) {
        pageLabel->setText(QString("%1 PAGE %2 / %3")
                               .arg(showingExLevels ? "EX STAGE" : "SELECT STAGE")
                               .arg(currentLevelSelectPage + 1)
                               .arg(pageCount));
    }
    if (prevButton) {
        prevButton->setEnabled(currentLevelSelectPage > 0);
    }
    if (nextButton) {
        nextButton->setEnabled(currentLevelSelectPage + 1 < pageCount);
    }
    if (normalButton && exButton) {
        normalButton->setProperty("levelMode", showingExLevels ? "true" : "active");
        exButton->setProperty("levelMode", showingExLevels ? "active" : "true");
        normalButton->style()->unpolish(normalButton);
        normalButton->style()->polish(normalButton);
        exButton->style()->unpolish(exButton);
        exButton->style()->polish(exButton);
    }

    if (selectedStageIndex < 0 || !isLevelUnlocked(selectedStageIndex)) {
        selectedStageIndex = -1;
        for (int i = 0; i < stagesPerPage; ++i) {
            const int stageIndex = firstIndex + i;
            if (stageIndex < visibleStages.size() && isLevelUnlocked(visibleStages.at(stageIndex).levelIndex)) {
                selectedStageIndex = visibleStages.at(stageIndex).levelIndex;
                break;
            }
        }
    }
    bool selectedStageVisible = false;
    for (int i = 0; i < 3; ++i) {
        const int stageIndex = firstIndex + i;
        const bool hasStage = stageIndex < visibleStages.size();
        const StageCard stage = hasStage ? visibleStages.at(stageIndex) : StageCard{-1, showingExLevels, "Empty", "", ""};
        const bool unlocked = hasStage && isLevelUnlocked(stage.levelIndex);
        const bool completed = hasStage && completedStageIndexes.contains(stage.levelIndex);
        buttons[i]->setEnabled(hasStage);
        buttons[i]->setProperty("levelIndex", stage.levelIndex);
        const bool selected = hasStage && stage.levelIndex == selectedStageIndex;
        buttons[i]->setProperty("levelCard", selected ? "selected" : (completed ? "cleared" : (unlocked ? "true" : "locked")));
        buttons[i]->style()->unpolish(buttons[i]);
        buttons[i]->style()->polish(buttons[i]);
        buttons[i]->setText(hasStage
                                ? (stage.isEx ? QString("EX\n%1").arg(stageIndex + 1)
                                              : QString::number(stageIndex + 1))
                                : "-");
        if (selected) {
            selectedStageVisible = true;
        }
    }
    for (int i = 0; i < 2; ++i) {
        if (!bridges[i]) {
            continue;
        }
        const int fromIndex = firstIndex + i;
        const int toIndex = firstIndex + i + 1;
        QString state = "locked";
        if (toIndex < visibleStages.size()) {
            const int fromLevel = visibleStages.at(fromIndex).levelIndex;
            const int toLevel = visibleStages.at(toIndex).levelIndex;
            state = completedStageIndexes.contains(fromLevel) ? "cleared" : (isLevelUnlocked(toLevel) ? "true" : "locked");
        }
        bridges[i]->setProperty("levelBridge", state);
        bridges[i]->style()->unpolish(bridges[i]);
        bridges[i]->style()->polish(bridges[i]);
    }
    if (!selectedStageVisible && selectedStageIndex >= 0) {
        selectedStageIndex = -1;
        refreshLevelSelectUi();
        return;
    }
    if (startButton) {
        startButton->setEnabled(selectedStageIndex >= 0 && !levels.isEmpty() && isLevelUnlocked(selectedStageIndex));
    }
    ui->mapBackButton->setText("Back");
}

bool MainWindow::isLevelUnlocked(int levelIndex) const
{
    if (levelIndex == 0 && !showingExLevels) {
        return true;
    }

    if (levelIndex >= 0 && levelIndex < levels.size()) {
        for (const LevelMeta &meta : const_cast<GameEngine &>(gameEngine).levelList()) {
            if (meta.levelIndex == levelIndex) {
                return meta.unlocked;
            }
        }
    }

    const QVector<StageCard> allStages = stageCatalog();
    for (int i = 0; i < allStages.size(); ++i) {
        if (allStages.at(i).levelIndex != levelIndex) {
            continue;
        }
        for (int previous = i - 1; previous >= 0; --previous) {
            if (allStages.at(previous).isEx != allStages.at(i).isEx) {
                break;
            }
            return completedStageIndexes.contains(allStages.at(previous).levelIndex);
        }
        return true;
    }
    return false;
}

void MainWindow::setLevelSelectMode(bool exMode)
{
    showingExLevels = exMode;
    currentLevelSelectPage = 0;
    selectedStageIndex = -1;
    refreshLevelSelectUi();
}

void MainWindow::changeLevelPage(int delta)
{
    currentLevelSelectPage += delta;
    selectedStageIndex = -1;
    refreshLevelSelectUi();
}

void MainWindow::selectStage(int levelIndex)
{
    if (!isLevelUnlocked(levelIndex)) {
        statusBar()->showMessage("Clear the previous connected stage first.", 2500);
        return;
    }
    selectedStageIndex = levelIndex;
    refreshLevelSelectUi();
}

void MainWindow::startLevel(int levelIndex)
{
    if (levelIndex < 0) {
        levelIndex = 0;
    }

    if (!isLevelUnlocked(levelIndex)) {
        statusBar()->showMessage("This stage is locked by the path order.", 2500);
        return;
    }
    if (levels.isEmpty()) {
        statusBar()->showMessage("This level is not available yet.", 2500);
        return;
    }

    if (levelIndex < 0 || levelIndex >= levels.size()) {
        statusBar()->showMessage("This stage has no JSON yet. Opening the preview map.", 2500);
        levelIndex = 0;
    }

    gameEngine.levels = levels;
    bool started = gameEngine.startLevel(levelIndex);
    if (!started && levelIndex == 0) {
        gameEngine.m_save.Unlock(0);
        gameEngine.m_save.Save();
        started = gameEngine.startLevel(levelIndex);
    }

    if (!started) {
        statusBar()->showMessage("Backend refused to start this level.", 2500);
        return;
    }
    currentLevelIndex = levelIndex;
    syncFromEngineState();
    ui->stackedWidget->setCurrentWidget(ui->gamePage);
    syncBgmToCurrentPage();
    refreshGameUi();
    if (mapView) {
        mapView->setFocus(Qt::OtherFocusReason);
    }
}

void MainWindow::resetLevel()
{
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    gameEngine.levels = levels;
    gameEngine.startLevel(currentLevelIndex);
    syncFromEngineState();
    activeMovePath.clear();
    activeMovePathIndex = 0;
    movePlaybackActive = false;
    suppressNextMovePath = false;
    pendingMoveTargetRow = -1;
    pendingMoveTargetColumn = -1;
    ui->combatLogLabel->setText("Use WASD/arrow keys or click a reachable tile.");
    refreshGameUi();
}

void MainWindow::undo()
{
    clearDisplayedMovePath();
    if (!gameEngine.m_map || !gameEngine.undo()) {
        ui->combatLogLabel->setText("Nothing to undo yet.");
        refreshGameUi();
        return;
    }

    syncFromEngineState();
    ui->combatLogLabel->setText("Undo restored the previous state.");
    refreshGameUi();
}

void MainWindow::refreshGameUi()
{
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    if (!movePlaybackActive) {
        syncFromEngineState();
    }
    const LevelData &level = levels.at(currentLevelIndex);
    ui->hpLabel->setText(QString("Level: %1").arg(currentLevelIndex + 1));
    ui->goldLabel->setText(QString("Bag: %1/%2").arg(bagBlocks.size()).arg(level.bagSize));
    ui->floorLabel->setText(QString("Pos: %1,%2").arg(playerColumn).arg(playerRow));
    ui->nextChallengeButton->setEnabled(gameEngine.snapshotStack.size() > 1);
    refreshMapGrid();
    refreshSidePanel();
}

void MainWindow::syncFromEngineState()
{
    if (gameEngine.m_level) {
        currentLevelIndex = gameEngine.m_level->levelIndex;
    }

    if (!gameEngine.m_map || !gameEngine.m_bag || currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    const QPoint playerPos = gameEngine.m_map->playerPos();
    playerRow = playerPos.x();
    playerColumn = playerPos.y();

    bagBlocks.clear();
    knownCodeBlocks.clear();
    for (const CodeBlock &block : gameEngine.m_bag->bag()) {
        bagBlocks.append(block.blockId);
        knownCodeBlocks.insert(block.blockId, block);
    }
    openedChests.clear();
    collectedClues.clear();
    defeatedMonsters.clear();
    const LevelData &level = levels.at(currentLevelIndex);
    for (auto it = level.chests.cbegin(); it != level.chests.cend(); ++it) {
        const QPoint pos = it.value().pos;
        if (pos.x() >= 0 && pos.x() < gameEngine.m_map->cleared.size()
            && pos.y() >= 0 && pos.y() < gameEngine.m_map->cleared.at(pos.x()).size()
            && gameEngine.m_map->cleared.at(pos.x()).at(pos.y())) {
            openedChests.insert(it.key());
        }
    }
    for (auto it = level.clues.cbegin(); it != level.clues.cend(); ++it) {
        if (gameEngine.m_map->clueRevealed(it.key())) {
            collectedClues.insert(it.key());
        }
    }
    for (auto it = level.monsters.cbegin(); it != level.monsters.cend(); ++it) {
        const QPoint pos = it.value().pos;
        if (pos.x() >= 0 && pos.x() < gameEngine.m_map->cleared.size()
            && pos.y() >= 0 && pos.y() < gameEngine.m_map->cleared.at(pos.x()).size()
            && gameEngine.m_map->cleared.at(pos.x()).at(pos.y())) {
            defeatedMonsters.insert(it.key());
        }
    }
    if (level.boss.pos.x() >= 0 && level.boss.pos.x() < gameEngine.m_map->cleared.size()
        && level.boss.pos.y() >= 0 && level.boss.pos.y() < gameEngine.m_map->cleared.at(level.boss.pos.x()).size()
        && gameEngine.m_map->cleared.at(level.boss.pos.x()).at(level.boss.pos.y())) {
        defeatedMonsters.insert("boss");
    }
}

void MainWindow::refreshMapGrid()
{
    if (!mapView || currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    mapView->setLevel(&level);
    mapView->setPlayerPosition(QPoint(playerColumn, playerRow));
    mapView->setClearedObjects(openedChests, defeatedMonsters);
    mapView->setCollectedClues(collectedClues);
    mapView->setMovePath(activeMovePath, activeMovePathIndex);
}

void MainWindow::refreshSidePanel()
{
    const QString tileId = tileAt(playerRow, playerColumn);
    tileInfoLabel->setText(describeTile(tileId));
    ui->challengeTitleLabel->setText("Interaction");
    ui->answerLineEdit->setEnabled(false);
    ui->submitAnswerButton->setEnabled(false);

    if (tileId.startsWith("monster") || tileId == "boss") {
        const Monster monster = monsterByTile(tileId);
        seenMonsters.insert(tileId);
        ui->challengeTitleLabel->setText(QString("Fill %1").arg(monster.nickname.isEmpty() ? monster.name : monster.nickname));
        ui->challengeTextEdit->setPlainText(renderMonsterCode(monster));
        ui->answerLineEdit->setEnabled(!defeatedMonsters.contains(tileId));
        ui->submitAnswerButton->setEnabled(!defeatedMonsters.contains(tileId));
    } else if (tileId.startsWith("chest")) {
        const Chest chest = levels.at(currentLevelIndex).chests.value(tileId);
        QStringList lines;
        lines << QString("Chest: %1").arg(tileId)
              << QString("Forced pick: %1").arg(chest.forcedPick ? "yes" : "no")
              << QString("Repeat: %1").arg(chest.repeat ? "yes" : "no")
              << "Blocks:";
        for (const CodeBlock &block : chest.blocks) {
            QString formatted = formatCodeBlockForDisplay(block.code);
            formatted.replace('\n', "\n    ");
            lines << QString("  %1 = %2").arg(block.blockId, formatted);
        }
        ui->challengeTextEdit->setPlainText(lines.join('\n'));
    } else if (tileId.startsWith("clue")) {
        const Clue clue = levels.at(currentLevelIndex).clues.value(tileId);
        ui->challengeTextEdit->setPlainText(clue.val.isEmpty() ? "No clue text." : clue.val);
    } else {
        ui->challengeTextEdit->setPlainText("Move onto a chest, clue, monster, or boss tile and interact.");
    }
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
                const bool defeated = defeatedMonsters.contains(monsterId);
                card->setText(QString("%1%2").arg(monsterDisplayName(monsterId, monster),
                                                  monsterId == "boss" ? "\nBOSS" : QString()));
                card->setIcon(QIcon(spriteForMonster(monsterId, monster)));
                if (defeated) {
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
                                           defeated ? "Defeated" : "Not defeated"));
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

    addStat(0, 0, "Clues Revealed", QString("%1 / %2").arg(collectedClues.size()).arg(level.clues.size()));
    addStat(0, 1, "Unlocked", isLevelUnlocked(currentLevelIndex + 1) ? "Next Stage" : "Current Stage");
    summaryLayout->addWidget(stats);

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
}

void MainWindow::movePlayer(int rowDelta, int columnDelta)
{
    if (movePlaybackActive) {
        return;
    }
    clearDisplayedMovePath();
    if (!gameEngine.m_map) {
        ui->combatLogLabel->setText("No active map.");
        return;
    }

    const QPoint backendPos = gameEngine.m_map->playerPos();
    if (!moveThroughEngine(backendPos.x() + rowDelta, backendPos.y() + columnDelta)) {
        ui->combatLogLabel->setText("Blocked.");
    }
}

void MainWindow::movePlayerTo(int targetRow, int targetColumn)
{
    if (movePlaybackActive) {
        return;
    }
    if (!gameEngine.m_map || currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        ui->combatLogLabel->setText("No path to that tile.");
        return;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    if (targetRow < 0 || targetRow >= level.mapGrid.size()
        || targetColumn < 0 || targetColumn >= level.mapGrid.at(targetRow).size()
        || !gameEngine.m_map->canGoIn(targetRow, targetColumn)) {
        ui->combatLogLabel->setText("No path to that tile.");
        return;
    }

    bool success = false;
    const QVector<QPoint> backendPath = gameEngine.m_map->findPath(targetRow, targetColumn, &success);
    if (!success || backendPath.size() < 2) {
        if (!moveThroughEngine(targetRow, targetColumn)) {
            ui->combatLogLabel->setText("No path to that tile.");
        }
        return;
    }

    startMovePlayback(backendPath, targetRow, targetColumn);
}

void MainWindow::startMovePlayback(const QVector<QPoint> &backendPath, int targetRow, int targetColumn)
{
    activeMovePath.clear();
    for (const QPoint &step : backendPath) {
        activeMovePath.append(QPoint(step.y(), step.x()));
    }
    activeMovePathIndex = 1;
    pendingMoveTargetRow = targetRow;
    pendingMoveTargetColumn = targetColumn;
    movePlaybackActive = true;

    const QPoint start = backendPath.first();
    playerRow = start.x();
    playerColumn = start.y();
    ui->combatLogLabel->setText(QString("Moving to %1,%2.").arg(targetColumn).arg(targetRow));
    refreshGameUi();
    QTimer::singleShot(115, this, &MainWindow::advanceMovePlayback);
}

void MainWindow::advanceMovePlayback()
{
    if (!movePlaybackActive) {
        return;
    }

    if (activeMovePathIndex >= activeMovePath.size()) {
        movePlaybackActive = false;
        activeMovePath.clear();
        activeMovePathIndex = 0;
        const int targetRow = pendingMoveTargetRow;
        const int targetColumn = pendingMoveTargetColumn;
        pendingMoveTargetRow = -1;
        pendingMoveTargetColumn = -1;
        suppressNextMovePath = true;
        if (!moveThroughEngine(targetRow, targetColumn)) {
            suppressNextMovePath = false;
            syncFromEngineState();
            refreshGameUi();
            ui->combatLogLabel->setText("No path to that tile.");
        }
        return;
    }

    const QPoint next = activeMovePath.at(activeMovePathIndex);
    playerRow = next.y();
    playerColumn = next.x();
    ++activeMovePathIndex;
    refreshGameUi();
    QTimer::singleShot(115, this, &MainWindow::advanceMovePlayback);
}

bool MainWindow::moveThroughEngine(int targetRow, int targetColumn)
{
    if (!gameEngine.m_map || currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return false;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    if (targetRow < 0 || targetRow >= level.mapGrid.size()
        || targetColumn < 0 || targetColumn >= level.mapGrid.at(targetRow).size()) {
        return false;
    }

    if (!gameEngine.m_map->canGoIn(targetRow, targetColumn)) {
        return false;
    }

    return gameEngine.moveTo(targetRow, targetColumn);
}

void MainWindow::clearDisplayedMovePath()
{
    if (!movePlaybackActive && activeMovePath.isEmpty()) {
        return;
    }

    movePlaybackActive = false;
    pendingMoveTargetRow = -1;
    pendingMoveTargetColumn = -1;
    activeMovePath.clear();
    activeMovePathIndex = 0;
    refreshGameUi();
}

bool MainWindow::canEnter(int row, int column) const
{
    if (!gameEngine.m_map || currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return false;
    }
    const LevelData &level = levels.at(currentLevelIndex);
    return row >= 0
           && row < level.mapGrid.size()
           && column >= 0
           && column < level.mapGrid.at(row).size()
           && gameEngine.m_map->canGoIn(row, column);
}

QString MainWindow::tileAt(int row, int column) const
{
    if (!canEnter(row, column)) {
        return "#";
    }
    return gameEngine.m_map ? gameEngine.m_map->currentId(row, column) : levels.at(currentLevelIndex).mapGrid.at(row).at(column);
}

QString MainWindow::describeTile(const QString &tileId) const
{
    if (tileId == "#") {
        return "Wall";
    }
    if (tileId == ".") {
        return "Room";
    }
    if (tileId == "start") {
        return "Spawn";
    }
    if (tileId == "boss") {
        return defeatedMonsters.contains("boss") ? "Cleared\nBoss" : "Boss\nGate";
    }
    if (tileId.startsWith("monster")) {
        return defeatedMonsters.contains(tileId) ? "Cleared\nFight" : "Enemy\n" + tileId.mid(QString("monster_").size());
    }
    if (tileId.startsWith("chest")) {
        return openedChests.contains(tileId) ? "Opened\nChest" : "Chest\n" + tileId.mid(QString("chest_").size());
    }
    if (tileId.startsWith("clue")) {
        return "Clue\n" + tileId.mid(QString("clue_").size());
    }
    return tileId;
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
                slot->setToolTip(tokenId);
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
        }
        syncFromEngineState();
        refreshGameUi();
        ui->combatLogLabel->setText(wonBoss ? levels.at(currentLevelIndex).endText : QString("%1 defeated.").arg(monsterId));
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
