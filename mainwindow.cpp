#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mapview.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDrag>
#include <QEvent>
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
#include <QQueue>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSlider>
#include <QStyle>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
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
        setAlignment(Qt::AlignCenter);
        setMinimumSize(132, 32);
        setProperty("blockId", QString());
        setText("              ");
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

        setProperty("blockId", blockId);
        setText(m_textProvider ? m_textProvider(blockId) : blockId);
        refreshStyle(false);
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
            "font-family: Consolas, 'Microsoft YaHei UI'; font-size: 15px; font-weight: 700; padding: 2px 9px; }"
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
    loadLevels();
    refreshLevelSelectUi();
    ui->stackedWidget->setCurrentWidget(ui->mainMenuPage);
    mainMenuButtonBar->show();
    mainMenuButtonBar->raise();
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
        cancelAutoMove(true);
        showBagDialog();
    });

    connect(ui->deckBackButton, &QPushButton::clicked, this, [this]() {
        if (!manualSelectedMonsterId.isEmpty()) {
            manualSelectedMonsterId.clear();
            refreshManualPage();
        } else {
            ui->stackedWidget->setCurrentWidget(ui->gamePage);
        }
    });

    connect(ui->deckListWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!manualSelectedMonsterId.isEmpty()) return;
        const QString monsterId = item->data(Qt::UserRole).toString();
        if (monsterId.isEmpty()) return;
        manualSelectedMonsterId = monsterId;
        refreshManualPage();
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

    connect(ui->submitAnswerButton, &QPushButton::clicked, this, [this]() {
        submitFill();
    });

    connect(ui->answerLineEdit, &QLineEdit::returnPressed, this, [this]() {
        submitFill();
    });

    connect(ui->nextChallengeButton, &QPushButton::clicked, this, [this]() {
        undo();
    });

    connect(resetRunButton, &QPushButton::clicked, this, [this]() {
        resetLevel();
    });

    connect(ui->manualButton, &QPushButton::clicked, this, [this]() {
        cancelAutoMove(true);
        showManualDialog();
    });

    connect(ui->manualBackButton, &QPushButton::clicked, this, [this]() {
        if (!manualSelectedMonsterId.isEmpty()) {
            manualSelectedMonsterId.clear();
            refreshManualPage();
        } else {
            ui->stackedWidget->setCurrentWidget(ui->gamePage);
        }
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
    });
    connect(ui->musicVolumeSlider, &QSlider::valueChanged, this, [this, updatePercent]() {
        updatePercent(ui->musicVolumeSlider, ui->musicVolumeValueLabel);
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

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (ui->stackedWidget->currentWidget() == ui->gamePage) {
        switch (event->key()) {
        case Qt::Key_W:
        case Qt::Key_Up:
            movePlayer(-1, 0);
            return;
        case Qt::Key_S:
        case Qt::Key_Down:
            movePlayer(1, 0);
            return;
        case Qt::Key_A:
        case Qt::Key_Left:
            movePlayer(0, -1);
            return;
        case Qt::Key_D:
        case Qt::Key_Right:
            movePlayer(0, 1);
            return;
        case Qt::Key_Z:
            if (event->modifiers() & Qt::ControlModifier) {
                undo();
                return;
            }
            break;
        default:
            break;
        }
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

        QWidget#deckPage {
            background: #16120d;
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

        QListWidget#deckListWidget {
            background: rgba(18, 14, 10, 238);
            border: 4px solid #120e09;
            border-radius: 6px;
            padding: 16px;
            outline: none;
        }

        QListWidget#deckListWidget::item {
            background: transparent;
            border: none;
            margin: 4px;
        }

        QLabel#deckTitleLabel, QLabel#settingsTitleLabel {
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
            border-radius: 38px;
            color: #78f6ff;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 17px;
            font-weight: 900;
            padding: 4px;
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
            border-radius: 38px;
            color: #fff2ff;
            font-family: "Georgia", "Times New Roman", "Microsoft YaHei UI";
            font-size: 16px;
            font-weight: 900;
            padding: 4px;
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
    ui->challengeFrame->hide();
    ui->mapButton->hide();
    ui->combatLogLabel->hide();
    ui->nextChallengeButton->hide();
    ui->bottomBarLayout->setStretch(0, 1);
    ui->bottomBarLayout->setStretch(1, 1);

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
    modeLayout->setSpacing(160);
    QPushButton *normalButton = new QPushButton("NORMAL", ui->mapFrame);
    QPushButton *exButton = new QPushButton("EX", ui->mapFrame);
    normalButton->setFixedSize(76, 76);
    exButton->setFixedSize(76, 76);
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

void MainWindow::loadLevels()
{
    QStringList errors;
    for (const QString &path : fallbackLevelPaths()) {
        QVector<LevelData> loaded = LoadDirectory(path, &errors);
        if (!loaded.isEmpty()) {
            levels = loaded;
            statusBar()->showMessage(QString("Loaded %1 level(s) from %2").arg(levels.size()).arg(path), 3000);
            return;
        }
    }

    LevelData single;
    QString error;
    const QString examplePath = QDir::current().filePath("example.json");
    if (single.LoadFromJson(examplePath, &error)) {
        levels.append(single);
        statusBar()->showMessage("Loaded example.json.", 3000);
        return;
    }

    levels.append(createPreviewLevel());
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

    currentLevelIndex = levelIndex;
    resetLevel();
    ui->stackedWidget->setCurrentWidget(ui->gamePage);
}

void MainWindow::resetLevel()
{
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    playerRow = 0;
    playerColumn = 0;
    for (int row = 0; row < level.mapGrid.size(); ++row) {
        for (int column = 0; column < level.mapGrid.at(row).size(); ++column) {
            if (level.mapGrid.at(row).at(column) == "start") {
                playerRow = row;
                playerColumn = column;
            }
        }
    }

    bagBlocks.clear();
    knownCodeBlocks.clear();
    remainingChestBlocks.clear();
    for (auto chestIt = level.chests.cbegin(); chestIt != level.chests.cend(); ++chestIt) {
        QSet<QString> remainingIds;
        for (auto blockIt = chestIt.value().blocks.cbegin(); blockIt != chestIt.value().blocks.cend(); ++blockIt) {
            remainingIds.insert(blockIt.key());
        }
        remainingChestBlocks.insert(chestIt.key(), remainingIds);
    }
    collectedClues.clear();
    openedChests.clear();
    defeatedMonsters.clear();
    seenMonsters.clear();
    history.clear();
    previousPlayerRow = playerRow;
    previousPlayerColumn = playerColumn;
    activeMovePath.clear();
    activeMovePathIndex = 0;
    autoMoving = false;
    ui->combatLogLabel->setText("Use WASD/arrow keys or click a reachable tile.");
    refreshGameUi();
}

void MainWindow::pushUndoState()
{
    UiGameSnapshot snapshot;
    snapshot.row = playerRow;
    snapshot.column = playerColumn;
    snapshot.bagBlocks = bagBlocks;
    snapshot.knownCodeBlocks = knownCodeBlocks;
    snapshot.remainingChestBlocks = remainingChestBlocks;
    snapshot.collectedClues = collectedClues;
    snapshot.openedChests = openedChests;
    snapshot.defeatedMonsters = defeatedMonsters;
    snapshot.seenMonsters = seenMonsters;
    history.append(snapshot);
}

void MainWindow::undo()
{
    if (history.isEmpty()) {
        ui->combatLogLabel->setText("Nothing to undo yet.");
        return;
    }

    const UiGameSnapshot snapshot = history.takeLast();
    playerRow = snapshot.row;
    playerColumn = snapshot.column;
    bagBlocks = snapshot.bagBlocks;
    knownCodeBlocks = snapshot.knownCodeBlocks;
    remainingChestBlocks = snapshot.remainingChestBlocks;
    collectedClues = snapshot.collectedClues;
    openedChests = snapshot.openedChests;
    defeatedMonsters = snapshot.defeatedMonsters;
    seenMonsters = snapshot.seenMonsters;
    ui->combatLogLabel->setText("Undo restored the previous state.");
    refreshGameUi();
}

void MainWindow::refreshGameUi()
{
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    ui->hpLabel->setText(QString("Level: %1").arg(currentLevelIndex + 1));
    ui->goldLabel->setText(QString("Bag: %1/%2").arg(bagBlocks.size()).arg(level.bagSize));
    ui->floorLabel->setText(QString("Pos: %1,%2").arg(playerColumn).arg(playerRow));
    ui->nextChallengeButton->setEnabled(!history.isEmpty());
    refreshMapGrid();
    refreshSidePanel();
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
            lines << QString("  %1 = %2").arg(block.blockId, block.code);
        }
        ui->challengeTextEdit->setPlainText(lines.join('\n'));
    } else if (tileId.startsWith("clue")) {
        const Clue clue = levels.at(currentLevelIndex).clues.value(tileId);
        ui->challengeTextEdit->setPlainText(clue.val.isEmpty() ? "No clue text." : clue.val);
    } else {
        ui->challengeTextEdit->setPlainText("Move onto a chest, clue, monster, or boss tile and interact.");
    }
}

void MainWindow::refreshBagPage()
{
    ui->deckTitleLabel->setText("Bag");
    ui->deckListWidget->clear();
    ui->deckListWidget->setStyleSheet("QListWidget#deckListWidget { background: rgba(18, 14, 10, 238); border: 4px solid #120e09; border-radius: 6px; padding: 10px; outline: none; }");
    ui->deckListWidget->setSpacing(0);
    ui->deckListWidget->setSelectionMode(QAbstractItemView::NoSelection);

    QVector<QPair<QString, QString>> codeItems;
    if (currentLevelIndex >= 0 && currentLevelIndex < levels.size()) {
        const LevelData &level = levels.at(currentLevelIndex);
        for (const QString &blockId : bagBlocks) {
            QString code = blockId;
            for (const Chest &chest : level.chests) {
                if (chest.blocks.contains(blockId)) {
                    code = chest.blocks.value(blockId).code;
                    break;
                }
            }
            codeItems.append(qMakePair(blockId, code));
        }
    }

    if (codeItems.isEmpty()) {
        codeItems.append(qMakePair(QString("waiting"), QString("open chest")));
    }

    constexpr int slotsPerPage = 5;
    constexpr int bagPageCount = 2;
    currentBagPage = qBound(0, currentBagPage, bagPageCount - 1);
    const int pageStart = currentBagPage * slotsPerPage;

    const auto makeCodeStrip = [](const QString &id, const QString &code, QWidget *parent) {
        QLabel *strip = new QLabel(parent);
        strip->setProperty("codeStrip", "true");
        strip->setWordWrap(true);
        strip->setAlignment(Qt::AlignCenter);
        if (id.isEmpty()) {
            strip->setText(" ");
            strip->setStyleSheet("background: rgba(244, 218, 166, 210); border: 2px solid #20160d; border-radius: 4px;");
        } else {
            strip->setText(QString("%1\n%2").arg(id, code));
        }
        return strip;
    };

    QFrame *board = new QFrame(ui->deckListWidget);
    board->setObjectName("bagBoardFrame");
    board->setProperty("bagBoard", "true");
    board->setMinimumSize(1180, 665);
    board->setStyleSheet("QFrame#bagBoardFrame { border-image: url(:/images/assets/bag_background.png) 0 0 0 0 stretch stretch; border: none; }");

    QPushButton *prevPage = new QPushButton("<", board);
    QPushButton *nextPage = new QPushButton(">", board);
    QLabel *pageLabel = new QLabel(QString("%1 / %2").arg(currentBagPage + 1).arg(bagPageCount), board);
    prevPage->setProperty("bagPageButton", "true");
    nextPage->setProperty("bagPageButton", "true");
    prevPage->setGeometry(930, 50, 46, 32);
    nextPage->setGeometry(1035, 50, 46, 32);
    pageLabel->setGeometry(980, 50, 52, 32);
    pageLabel->setAlignment(Qt::AlignCenter);
    pageLabel->setProperty("bagSection", "true");
    prevPage->setEnabled(currentBagPage > 0);
    nextPage->setEnabled(currentBagPage + 1 < bagPageCount);
    connect(prevPage, &QPushButton::clicked, this, [this]() {
        currentBagPage = qMax(0, currentBagPage - 1);
        refreshBagPage();
    });
    connect(nextPage, &QPushButton::clicked, this, [this]() {
        currentBagPage = qMin(1, currentBagPage + 1);
        refreshBagPage();
    });

    const QVector<QRect> stripRects = {
        QRect(212, 94, 745, 78),
        QRect(212, 195, 745, 78),
        QRect(212, 296, 745, 78),
        QRect(212, 397, 745, 78),
        QRect(212, 498, 745, 78)
    };
    for (int i = 0; i < stripRects.size(); ++i) {
        const int itemIndex = pageStart + i;
        QLabel *strip = itemIndex < codeItems.size()
                            ? makeCodeStrip(codeItems.at(itemIndex).first, codeItems.at(itemIndex).second, board)
                            : makeCodeStrip(QString(), QString(), board);
        strip->setGeometry(stripRects.at(i));
        strip->show();
    }

    QListWidgetItem *item = new QListWidgetItem(ui->deckListWidget);
    item->setSizeHint(QSize(1180, 665));
    ui->deckListWidget->setItemWidget(item, board);
}

void MainWindow::refreshManualPage()
{
    //---pageInit---
    ui->manualListWidget->clear();
    QString manualListStyle =
        "QListWidget#deckListWidget { "
        "background: rgba(16, 18, 24, 255); "
        "border: 2px solid #485162; "
        "border-radius: 6px; "
        "padding: 18px; "
        "outline: none; "
        "} "
        "QListWidget#deckListWidget::item { "
        "padding: 8px 10px; "
        "border-radius: 4px; "
        "color: #E0E0E0; "
        "} "
        "QListWidget#deckListWidget::item:hover { "
        "background-color: rgba(72, 81, 98, 150); "
        "} "
        "QListWidget#deckListWidget::item:selected { "
        "background-color: #485162; "
        "color: white; "
        "}";
    ui->manualListWidget->setStyleSheet(manualListStyle);
    ui->manualListWidget->setSpacing(6);

    if(currentLevelIndex < 0 || currentLevelIndex > levels.size()){
        return;
    }
    const LevelData &level = levels.at(currentLevelIndex);

    // --- detail view ---
    if (!manualSelectedMonsterId.isEmpty()) {
        if (!level.monsters.contains(manualSelectedMonsterId)) {
            manualSelectedMonsterId.clear();
            refreshManualPage();
            return;
        }

        const Monster &m = level.monsters[manualSelectedMonsterId];
        const bool isBoss = (manualSelectedMonsterId == "boss");
        const bool seen = seenMonsters.contains(manualSelectedMonsterId)
                       || defeatedMonsters.contains(manualSelectedMonsterId);
        const bool defeated = defeatedMonsters.contains(manualSelectedMonsterId);

        ui->deckTitleLabel->setText(seen ? (m.nickname.isEmpty() ? m.name : m.nickname) : "???");

        QFrame *detailCard = new QFrame();
        detailCard->setObjectName("manualDetailCard");
        detailCard->setStyleSheet(
            "QFrame#manualDetailCard { background: #1a1d26; border: 2px solid #353a47; border-radius: 8px; }"
        );

        QVBoxLayout *root = new QVBoxLayout(detailCard);
        root->setContentsMargins(20, 18, 20, 18);
        root->setSpacing(14);

        if (!seen) {
            QLabel *unknown = new QLabel("???  —  Undiscovered");
            unknown->setStyleSheet("color: #5b5f6b; font-size: 22px; font-weight: bold;");
            unknown->setAlignment(Qt::AlignCenter);
            root->addWidget(unknown);
            QLabel *hint = new QLabel("Encounter this enemy in the tower to reveal its details.");
            hint->setStyleSheet("color: #464a55; font-size: 13px;");
            hint->setAlignment(Qt::AlignCenter);
            hint->setWordWrap(true);
            root->addWidget(hint);
        } else {
            // header row: sprite + badges
            QHBoxLayout *headerRow = new QHBoxLayout();
            headerRow->setSpacing(18);

            static const QStringList monsterSprites = {
                ":/images/assets/alice.png", ":/images/assets/cheshire_cat.png",
                ":/images/assets/dodo.png", ":/images/assets/gerda.png",
                ":/images/assets/jabberwock.png", ":/images/assets/mabel.png",
                ":/images/assets/node.png", ":/images/assets/red_hood.png"
            };
            QString spriteFile = isBoss
                ? QString(":/images/assets/marry_ann.png")
                : monsterSprites.at(qHash(manualSelectedMonsterId) % monsterSprites.size());

            QLabel *spriteLabel = new QLabel();
            spriteLabel->setFixedSize(112, 112);
            spriteLabel->setAlignment(Qt::AlignCenter);
            QPixmap pix(spriteFile);
            if (!pix.isNull()) {
                spriteLabel->setPixmap(pix.scaled(104, 104, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                spriteLabel->setText(isBoss ? "BOSS" : "M");
                spriteLabel->setStyleSheet("color: #d99cff; font-size: 20px; font-weight: bold; background: #2d2040; border-radius: 6px;");
            }
            headerRow->addWidget(spriteLabel);

            QWidget *headerInfo = new QWidget();
            QVBoxLayout *hiLayout = new QVBoxLayout(headerInfo);
            hiLayout->setContentsMargins(0, 0, 0, 0);
            hiLayout->setSpacing(5);

            QLabel *nameLabel = new QLabel(m.nickname.isEmpty() ? m.name : m.nickname);
            nameLabel->setStyleSheet("color: #f1d37c; font-size: 22px; font-weight: bold;");
            hiLayout->addWidget(nameLabel);

            QHBoxLayout *badgeRow = new QHBoxLayout();
            badgeRow->setSpacing(8);
            QString typeColor = (m.type == "function") ? "#5dade2"
                              : (m.type == "class") ? "#58d68d"
                              : "#d99cff";
            QLabel *typeBadge = new QLabel(m.type.toUpper());
            typeBadge->setStyleSheet(QString(
                "color: %1; font-size: 11px; font-weight: bold;"
                "background: transparent; border: 1px solid %1;"
                "border-radius: 3px; padding: 2px 8px;"
            ).arg(typeColor));
            badgeRow->addWidget(typeBadge);

            if (defeated) {
                QLabel *defBadge = new QLabel("DEFEATED");
                defBadge->setStyleSheet(
                    "color: #f6d878; font-size: 11px; font-weight: bold;"
                    "background: transparent; border: 1px solid #f6d878;"
                    "border-radius: 3px; padding: 2px 8px;"
                );
                badgeRow->addWidget(defBadge);
            }
            QLabel *idLabel = new QLabel(manualSelectedMonsterId);
            idLabel->setStyleSheet("color: #5e6370; font-size: 11px;");
            badgeRow->addWidget(idLabel);
            badgeRow->addStretch();
            hiLayout->addLayout(badgeRow);

            if (isBoss) {
                const Boss &boss = level.boss;
                if (!boss.input.isEmpty() || !boss.output.isEmpty()) {
                    QLabel *ioLabel = new QLabel(QString("Input: %1    Output: %2").arg(boss.input, boss.output));
                    ioLabel->setStyleSheet("color: #78c8e2; font-size: 12px; font-family: 'Consolas', monospace;");
                    hiLayout->addWidget(ioLabel);
                }
            }

            hiLayout->addStretch();
            headerRow->addWidget(headerInfo, 1);
            root->addLayout(headerRow);

            // code template
            QLabel *codeSection = new QLabel("Code Template");
            codeSection->setStyleSheet("color: #8e94a0; font-size: 12px; font-weight: bold; margin-top: 4px;");
            root->addWidget(codeSection);

            QString renderedCode = renderMonsterCode(m);
            QLabel *codeLabel = new QLabel(renderedCode);
            codeLabel->setWordWrap(true);
            codeLabel->setTextFormat(Qt::PlainText);
            codeLabel->setStyleSheet(
                "color: #c8d6e5; font-family: 'Consolas', 'Cascadia Mono', 'Courier New', monospace;"
                "font-size: 13px; background: #11131a; border: 1px solid #282c36;"
                "border-radius: 4px; padding: 10px 12px;"
            );
            root->addWidget(codeLabel);

            // spaces table
            if (!m.spaces.isEmpty()) {
                QLabel *spacesSection = new QLabel("Spaces");
                spacesSection->setStyleSheet("color: #8e94a0; font-size: 12px; font-weight: bold; margin-top: 4px;");
                root->addWidget(spacesSection);

                QFrame *spacesTable = new QFrame();
                spacesTable->setStyleSheet("QFrame { background: #11131a; border: 1px solid #282c36; border-radius: 4px; }");
                QVBoxLayout *spacesLayout = new QVBoxLayout(spacesTable);
                spacesLayout->setContentsMargins(12, 8, 12, 8);
                spacesLayout->setSpacing(4);

                for (const Space &sp : m.spaces) {
                    QHBoxLayout *spaceRow = new QHBoxLayout();
                    spaceRow->setSpacing(10);

                    QLabel *idLbl = new QLabel(sp.spaceId);
                    idLbl->setStyleSheet("color: #f1d37c; font-size: 12px; font-weight: bold; font-family: 'Consolas', monospace;");
                    idLbl->setFixedWidth(100);
                    spaceRow->addWidget(idLbl);

                    QLabel *typeLbl = new QLabel(sp.type);
                    typeLbl->setStyleSheet("color: #78c8e2; font-size: 11px; font-family: 'Consolas', monospace;");
                    typeLbl->setFixedWidth(60);
                    spaceRow->addWidget(typeLbl);

                    QLabel *valLbl = new QLabel(sp.values.join(", "));
                    valLbl->setStyleSheet("color: #a0a8b8; font-size: 11px; font-family: 'Consolas', monospace;");
                    valLbl->setWordWrap(true);
                    spaceRow->addWidget(valLbl, 1);

                    spacesLayout->addLayout(spaceRow);
                }
                root->addWidget(spacesTable);
            }

            // referenced clues
            if (!m.referencedClues.isEmpty()) {
                QLabel *cluesSection = new QLabel("Referenced Clues");
                cluesSection->setStyleSheet("color: #8e94a0; font-size: 12px; font-weight: bold; margin-top: 4px;");
                root->addWidget(cluesSection);

                QFrame *cluesFrame = new QFrame();
                cluesFrame->setStyleSheet("QFrame { background: #11131a; border: 1px solid #282c36; border-radius: 4px; }");
                QVBoxLayout *cluesLayout = new QVBoxLayout(cluesFrame);
                cluesLayout->setContentsMargins(12, 8, 12, 8);
                cluesLayout->setSpacing(4);

                for (const QString &clueId : m.referencedClues) {
                    QString clueText = level.clues.contains(clueId) ? level.clues[clueId].val : "(missing)";
                    QLabel *clueLine = new QLabel(QString("%1  →  %2").arg(clueId, clueText.trimmed()));
                    clueLine->setStyleSheet("color: #78e2a0; font-size: 12px; font-family: 'Consolas', monospace;");
                    clueLine->setWordWrap(true);
                    cluesLayout->addWidget(clueLine);
                }
                root->addWidget(cluesFrame);
            }
        }

        root->addStretch();

        QListWidgetItem *detailItem = new QListWidgetItem();
        detailItem->setFlags(Qt::NoItemFlags);
        ui->deckListWidget->addItem(detailItem);
        ui->deckListWidget->setItemWidget(detailItem, detailCard);
        return;
    }

    //---list view---
    ui->manualTitleLabel->setText("Monster Manual");
    ui->manualListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    QString cardStyle =
        "QFrame {"
        "background-color: #2b2d30;"
        "border-radius: 8px;"
        "border: 1px solid #44474a;"
        "padding: 5px;"
        "}";
    for(auto it = level.monsters.constBegin(); it != level.monsters.constEnd(); ++it){
        const Monster &m = it.value();
        const bool isSeen = seenMonsters.contains(m.monsterId) || defeatedMonsters.contains(m.monsterId);

        //---init card for monsters---
        QFrame *card = new QFrame();
        card->setObjectName(m.nickname);
        card->setStyleSheet(cardStyle);

        QHBoxLayout *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(8, 8, 8, 8);
        cardLayout->setSpacing(10);
        QLabel *iconLabel = new QLabel(card);
        iconLabel->setFixedSize(64, 64);
        iconLabel->setScaledContents(true);
        QString imgPath = ":/images/assets/" + m.pic;
        iconLabel->setPixmap(imgPath);
        cardLayout->addWidget(iconLabel);
        QWidget *textContainer = new QWidget(card);
        QVBoxLayout *textLayout = new QVBoxLayout(textContainer);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(4);
        QLabel *nameLabel = new QLabel(m.nickname, textContainer);
        nameLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #FFFFFF;");
        QLabel *descLabel = new QLabel(isSeen ? m.name : "???", textContainer);
        descLabel->setStyleSheet("font-size: 12px; color: #AAAAAA;");
        textLayout->addWidget(nameLabel);
        textLayout->addWidget(descLabel);
        cardLayout->addWidget(textContainer, 1);

        QListWidgetItem *item = new QListWidgetItem(ui->manualListWidget);
        item->setSizeHint(card->sizeHint());
        ui->manualListWidget->setItemWidget(item, card);
        item->setData(Qt::UserRole, m.monsterId);
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
            icon->setToolTip(blockId);
            connect(icon, &QToolButton::clicked, &dialog, [&dialog, blockId, code]() {
                QDialog detail(&dialog);
                detail.setWindowTitle(blockId);
                QVBoxLayout *detailLayout = new QVBoxLayout(&detail);
                QLabel *name = new QLabel(blockId, &detail);
                QFont nameFont = name->font();
                nameFont.setPointSize(16);
                nameFont.setBold(true);
                name->setFont(nameFont);
                name->setAlignment(Qt::AlignCenter);
                QTextBrowser *codeView = new QTextBrowser(&detail);
                codeView->setReadOnly(true);
                codeView->setHtml(QString("<pre style=\"font-family:Consolas; font-size:15px; color:#f9f1d0; background:#111827; padding:14px;\">%1</pre>")
                                      .arg(code.toHtmlEscaped()));
                QDialogButtonBox *close = new QDialogButtonBox(QDialogButtonBox::Close, &detail);
                QObject::connect(close, &QDialogButtonBox::rejected, &detail, &QDialog::reject);
                detailLayout->addWidget(name);
                detailLayout->addWidget(codeView);
                detailLayout->addWidget(close);
                detail.resize(520, 360);
                detail.exec();
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

    auto showMonsterDetail = [this, &dialog, &spriteForMonster](const QString &monsterId, const Monster &monster) {
        QDialog detail(&dialog);
        detail.setWindowTitle(monster.nickname.isEmpty() ? monsterId : monster.nickname);
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
        QLabel *typeBadge = new QLabel(monster.type.toUpper(), portraitPanel);
        typeBadge->setAlignment(Qt::AlignCenter);
        typeBadge->setStyleSheet("QLabel { color: #4feaff; font-size: 18px; font-weight: 800; letter-spacing: 0px; }");
        portraitLayout->addWidget(typeBadge);
        boardLayout->addWidget(portraitPanel, 0);

        QFrame *infoPanel = new QFrame(board);
        infoPanel->setStyleSheet("QFrame { background: rgba(8, 10, 14, 178); border: 2px solid rgba(79, 234, 255, 145); border-radius: 8px; }");
        QVBoxLayout *infoLayout = new QVBoxLayout(infoPanel);
        infoLayout->setContentsMargins(22, 18, 22, 18);
        infoLayout->setSpacing(10);

        QLabel *name = new QLabel(monster.nickname.isEmpty() ? monster.name : monster.nickname, infoPanel);
        name->setStyleSheet("QLabel { color: #ffd86b; font-size: 28px; font-weight: 900; }");
        name->setWordWrap(true);
        infoLayout->addWidget(name);

        QLabel *idLine = new QLabel(QString("%1   |   %2").arg(monsterId, monster.name), infoPanel);
        idLine->setStyleSheet("QLabel { color: rgba(246, 231, 189, 190); font-size: 14px; }");
        idLine->setWordWrap(true);
        infoLayout->addWidget(idLine);

        QTextBrowser *codeView = new QTextBrowser(infoPanel);
        codeView->setHtml(renderMonsterCodeHtml(monster));
        codeView->setStyleSheet(
            "QTextBrowser { background: rgba(9, 16, 28, 220); border: 1px solid rgba(215, 176, 106, 150);"
            "border-radius: 6px; padding: 8px; color: #f9f1d0; }"
        );
        codeView->setMinimumHeight(210);
        infoLayout->addWidget(codeView, 1);

        QLabel *spaceTitle = new QLabel("Fill Rules", infoPanel);
        spaceTitle->setStyleSheet("QLabel { color: #4feaff; font-size: 15px; font-weight: 800; }");
        infoLayout->addWidget(spaceTitle);
        QStringList spaceLines;
        for (const Space &space : monster.spaces) {
            spaceLines << QString("%1  [%2]  %3")
                              .arg(space.spaceId,
                                   space.type.isEmpty() ? "match" : space.type,
                                   space.values.join(", "));
        }
        QLabel *spaces = new QLabel(spaceLines.isEmpty() ? "No fill slot." : spaceLines.join("\n"), infoPanel);
        spaces->setStyleSheet("QLabel { color: #f6e7bd; font-family: 'Consolas', monospace; font-size: 13px; }");
        spaces->setWordWrap(true);
        infoLayout->addWidget(spaces);

        QLabel *clueTitle = new QLabel("Clues", infoPanel);
        clueTitle->setStyleSheet("QLabel { color: #4feaff; font-size: 15px; font-weight: 800; }");
        infoLayout->addWidget(clueTitle);
        QStringList clueLines;
        if (currentLevelIndex >= 0 && currentLevelIndex < levels.size()) {
            const LevelData &level = levels.at(currentLevelIndex);
            for (const QString &clueId : monster.referencedClues) {
                const bool unlocked = collectedClues.contains(clueId);
                clueLines << QString("%1: %2")
                                  .arg(clueId,
                                       unlocked ? level.clues.value(clueId).val.trimmed() : hiddenCodeMask());
            }
        }
        QLabel *clues = new QLabel(clueLines.isEmpty() ? "No referenced clue." : clueLines.join("\n"), infoPanel);
        clues->setStyleSheet("QLabel { color: #a8f4bf; font-family: 'Consolas', monospace; font-size: 13px; }");
        clues->setWordWrap(true);
        infoLayout->addWidget(clues);

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
        for (auto it = level.monsters.cbegin(); it != level.monsters.cend(); ++it) {
            monsterItems.append(qMakePair(it.key(), it.value()));
        }
        if (!level.boss.monsterId.isEmpty()) {
            monsterItems.append(qMakePair(QString("boss"), static_cast<Monster>(level.boss)));
        }
    }

    const int itemsPerPage = 4;
    const int totalPages = qMax(2, (monsterItems.size() + itemsPerPage - 1) / itemsPerPage);
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
                card->setText(QString("%1\n%2\n%3")
                                  .arg(monster.nickname.isEmpty() ? monsterId : monster.nickname,
                                       monster.name,
                                       monster.type.toUpper()));
                card->setIcon(QIcon(spriteForMonster(monsterId, monster)));
                QStringList clueLines;
                if (currentLevelIndex >= 0 && currentLevelIndex < levels.size()) {
                    const LevelData &level = levels.at(currentLevelIndex);
                    for (const QString &clueId : monster.referencedClues) {
                        clueLines << QString("%1: %2")
                                         .arg(clueId.toHtmlEscaped(),
                                              collectedClues.contains(clueId)
                                                  ? level.clues.value(clueId).val.toHtmlEscaped()
                                                  : hiddenCodeMask());
                    }
                }
                installHoverPopup(card,
                                  QString("<b>%1</b><br>Type: %2<br>Clues:<br>%3")
                                      .arg((monster.nickname.isEmpty() ? monsterId : monster.nickname).toHtmlEscaped(),
                                           monster.type.toHtmlEscaped(),
                                           clueLines.isEmpty() ? "No referenced clue." : clueLines.join("<br>")));
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

    addStat(0, 0, "Collected Blocks", QString::number(bagBlocks.size()));
    addStat(0, 1, "Clues Revealed", QString("%1 / %2").arg(collectedClues.size()).arg(level.clues.size()));
    addStat(1, 0, "Enemies Cleared", QString("%1 / %2").arg(defeatedMonsters.size()).arg(level.monsters.size() + 1));
    addStat(1, 1, "Unlocked", isLevelUnlocked(currentLevelIndex + 1) ? "Next Stage" : "Current Stage");
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

    connect(stayButton, &QPushButton::clicked, &dialog, &QDialog::accept);
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
    if (autoMoving) {
        cancelAutoMove(true);
    }
    stepPlayerTo(playerRow + rowDelta, playerColumn + columnDelta, true);
}

void MainWindow::movePlayerTo(int targetRow, int targetColumn)
{
    if (autoMoving) {
        return;
    }
    if (!canEnter(targetRow, targetColumn)) {
        ui->combatLogLabel->setText("That tile is blocked.");
        return;
    }

    const LevelData &level = levels.at(currentLevelIndex);
    QVector<QVector<bool>> visited(level.mapGrid.size());
    QVector<QVector<QPoint>> parent(level.mapGrid.size());
    for (int row = 0; row < level.mapGrid.size(); ++row) {
        visited[row] = QVector<bool>(level.mapGrid.at(row).size(), false);
        parent[row] = QVector<QPoint>(level.mapGrid.at(row).size(), QPoint(-1, -1));
    }

    QQueue<QPoint> queue;
    queue.enqueue(QPoint(playerColumn, playerRow));
    visited[playerRow][playerColumn] = true;
    const QPoint directions[] = {QPoint(1, 0), QPoint(-1, 0), QPoint(0, 1), QPoint(0, -1)};
    while (!queue.isEmpty()) {
        const QPoint current = queue.dequeue();
        if (current.y() == targetRow && current.x() == targetColumn) {
            break;
        }
        for (const QPoint &direction : directions) {
            const int nextColumn = current.x() + direction.x();
            const int nextRow = current.y() + direction.y();
            if (canUseAsPathNode(nextRow, nextColumn, QPoint(targetColumn, targetRow)) && !visited[nextRow][nextColumn]) {
                visited[nextRow][nextColumn] = true;
                parent[nextRow][nextColumn] = current;
                queue.enqueue(QPoint(nextColumn, nextRow));
            }
        }
    }

    if (!visited[targetRow][targetColumn]) {
        ui->combatLogLabel->setText("No path to that tile.");
        return;
    }

    QVector<QPoint> path;
    QPoint cursor(targetColumn, targetRow);
    while (cursor != QPoint(playerColumn, playerRow)) {
        path.prepend(cursor);
        cursor = parent[cursor.y()][cursor.x()];
    }
    if (path.isEmpty()) {
        triggerTileEvent(false);
        return;
    }

    pushUndoState();
    activeMovePath = path;
    activeMovePathIndex = 0;
    autoMoving = true;
    advanceAutoMove();
}

void MainWindow::advanceAutoMove()
{
    if (!autoMoving) {
        return;
    }
    if (activeMovePathIndex >= activeMovePath.size()) {
        autoMoving = false;
        activeMovePath.clear();
        activeMovePathIndex = 0;
        triggerTileEvent(true);
        return;
    }

    const QPoint next = activeMovePath.at(activeMovePathIndex++);
    if (!stepPlayerTo(next.y(), next.x(), false)) {
        autoMoving = false;
        activeMovePath.clear();
        activeMovePathIndex = 0;
        ui->combatLogLabel->setText("Path was interrupted.");
        return;
    }

    const QString tileId = tileAt(playerRow, playerColumn);
    const bool finalStep = activeMovePathIndex >= activeMovePath.size();
    if (hasTileEvent(tileId) && (finalStep || !isSkippablePathChest(tileId))) {
        autoMoving = false;
        activeMovePath.clear();
        activeMovePathIndex = 0;
        triggerTileEvent(true);
        return;
    }

    QTimer::singleShot(115, this, &MainWindow::advanceAutoMove);
}

void MainWindow::cancelAutoMove(bool returnToPreviousPoint)
{
    if (!autoMoving && activeMovePath.isEmpty()) {
        return;
    }

    autoMoving = false;
    activeMovePath.clear();
    activeMovePathIndex = 0;
    if (returnToPreviousPoint) {
        playerRow = previousPlayerRow;
        playerColumn = previousPlayerColumn;
    }
    refreshGameUi();
    ui->combatLogLabel->setText("Movement interrupted.");
}

bool MainWindow::stepPlayerTo(int row, int column, bool saveUndo)
{
    if (!canEnter(row, column)) {
        ui->combatLogLabel->setText("Blocked.");
        return false;
    }

    if (saveUndo) {
        pushUndoState();
    }
    previousPlayerRow = playerRow;
    previousPlayerColumn = playerColumn;
    playerRow = row;
    playerColumn = column;
    ui->combatLogLabel->setText(QString("Moved to %1,%2.").arg(playerColumn).arg(playerRow));
    refreshGameUi();
    if (saveUndo) {
        triggerTileEvent(false);
    }
    return true;
}

bool MainWindow::canEnter(int row, int column) const
{
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return false;
    }
    const LevelData &level = levels.at(currentLevelIndex);
    return row >= 0
           && row < level.mapGrid.size()
           && column >= 0
           && column < level.mapGrid.at(row).size()
           && level.mapGrid.at(row).at(column) != "#";
}

bool MainWindow::canUseAsPathNode(int row, int column, const QPoint &target) const
{
    if (!canEnter(row, column)) {
        return false;
    }
    if (QPoint(column, row) == target) {
        return true;
    }

    const QString tileId = tileAt(row, column);
    if (tileId.startsWith("monster") || tileId == "boss") {
        return defeatedMonsters.contains(tileId);
    }
    if (tileId.startsWith("chest")) {
        return isSkippablePathChest(tileId) || openedChests.contains(tileId);
    }
    return true;
}

QString MainWindow::tileAt(int row, int column) const
{
    if (!canEnter(row, column)) {
        return "#";
    }
    return levels.at(currentLevelIndex).mapGrid.at(row).at(column);
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

bool MainWindow::hasTileEvent(const QString &tileId) const
{
    if ((tileId.startsWith("monster") || tileId == "boss") && defeatedMonsters.contains(tileId)) {
        return false;
    }
    if (tileId.startsWith("chest")) {
        if (openedChests.contains(tileId)) {
            return false;
        }
        return chestHasAvailableBlocks(tileId);
    }
    if (tileId.startsWith("clue")) {
        return !collectedClues.contains(tileId);
    }
    return tileId.startsWith("chest") || tileId.startsWith("monster") || tileId == "boss" || tileId.startsWith("clue");
}

bool MainWindow::isSkippablePathChest(const QString &tileId) const
{
    if (!tileId.startsWith("chest") || currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return false;
    }
    const Chest chest = levels.at(currentLevelIndex).chests.value(tileId);
    return !chest.forcedPick;
}

void MainWindow::triggerTileEvent(bool fromAutoMove)
{
    Q_UNUSED(fromAutoMove);
    const QString tileId = tileAt(playerRow, playerColumn);
    if (!hasTileEvent(tileId)) {
        return;
    }
    interactWithCurrentTile();
}

void MainWindow::returnToPreviousTile()
{
    playerRow = previousPlayerRow;
    playerColumn = previousPlayerColumn;
    refreshGameUi();
}

void MainWindow::interactWithCurrentTile()
{
    const QString tileId = tileAt(playerRow, playerColumn);
    if (tileId.startsWith("chest")) {
        handleChest(tileId);
    } else if (tileId.startsWith("monster") || tileId == "boss") {
        handleMonster(tileId);
    } else if (tileId.startsWith("clue")) {
        const Clue clue = levels.at(currentLevelIndex).clues.value(tileId);
        if (collectedClues.contains(tileId)) {
            ui->combatLogLabel->setText("Clue already recorded.");
            return;
        }
        pushUndoState();
        collectedClues.insert(tileId);
        QMessageBox::information(this,
                                 "Clue",
                                 clue.val.isEmpty() ? "No clue text." : clue.val);
        ui->combatLogLabel->setText(QString("%1 recorded in monster intel.").arg(tileId));
    } else {
        ui->combatLogLabel->setText("Nothing to interact with here.");
    }
    refreshGameUi();
}

void MainWindow::handleChest(const QString &chestId)
{
    const Chest chest = levels.at(currentLevelIndex).chests.value(chestId);
    if (!chestHasAvailableBlocks(chestId)) {
        openedChests.insert(chestId);
        ui->combatLogLabel->setText("This chest is already empty.");
        refreshGameUi();
        return;
    }
    if (openedChests.contains(chestId) && !chest.repeat) {
        ui->combatLogLabel->setText("This chest is already empty.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(chest.forcedPick ? "Forced Chest" : "Chest");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *summary = new QLabel(QString("%1\nForced: %2\nRepeat: %3")
                                     .arg(chestId,
                                          chest.forcedPick ? "yes" : "no",
                                          chest.repeat ? "yes" : "no"),
                                 &dialog);
    summary->setWordWrap(true);
    QWidget *contents = new QWidget(&dialog);
    QGridLayout *contentGrid = new QGridLayout(contents);
    contentGrid->setSpacing(12);
    QString selectedBlockId;
    int blockIndex = 0;
    QSet<QString> remainingIds = remainingChestBlocks.value(chestId);
    if (!remainingChestBlocks.contains(chestId)) {
        for (auto blockIt = chest.blocks.cbegin(); blockIt != chest.blocks.cend(); ++blockIt) {
            remainingIds.insert(blockIt.key());
        }
    }
    for (const CodeBlock &block : chest.blocks) {
        if (!remainingIds.contains(block.blockId)) {
            continue;
        }
        QToolButton *blockIcon = new QToolButton(contents);
        blockIcon->setFixedSize(128, 68);
        blockIcon->setToolButtonStyle(Qt::ToolButtonIconOnly);
        blockIcon->setIcon(QIcon(codeBlockIconPath(block.blockId)));
        blockIcon->setIconSize(QSize(58, 58));
        blockIcon->setToolTip(block.blockId);
        installHoverPopup(blockIcon,
                          QString("<b>%1</b><br><pre>%2</pre>")
                              .arg(block.blockId.toHtmlEscaped(), block.code.toHtmlEscaped()));
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
            pushUndoState();
            knownCodeBlocks[selectedBlockId] = chest.blocks.value(selectedBlockId);
            bagBlocks.append(selectedBlockId);
            remainingChestBlocks[chestId].remove(selectedBlockId);
            picked = selectedBlockId;
            if (!chest.repeat || remainingChestBlocks.value(chestId).isEmpty()) {
                openedChests.insert(chestId);
            }
        }
        ui->combatLogLabel->setText(picked.isEmpty()
                                        ? "No new block was picked."
                                        : QString("Picked: %1").arg(picked));
    } else {
        ui->combatLogLabel->setText(chest.forcedPick ? "Forced chest closed. Returned to the previous tile." : "Chest skipped.");
    }

    if (chest.forcedPick) {
        returnToPreviousTile();
    }
}

void MainWindow::handleMonster(const QString &monsterId)
{
    seenMonsters.insert(monsterId);
    if (defeatedMonsters.contains(monsterId)) {
        ui->combatLogLabel->setText("This enemy is already defeated.");
        return;
    }

    const Monster monster = monsterByTile(monsterId);
    QDialog dialog(this);
    dialog.setWindowTitle(monsterId == "boss" ? "Boss Encounter" : "Monster Encounter");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *title = new QLabel(QString("%1  %2")
                                   .arg(monster.nickname.isEmpty() ? monster.name : monster.nickname,
                                        monster.type),
                               &dialog);
    title->setWordWrap(true);
    QLabel *hero = new QLabel(&dialog);
    hero->setFixedSize(180, 220);
    hero->setAlignment(Qt::AlignCenter);
    hero->setPixmap(QPixmap(":/images/assets/jibao.png").scaled(hero->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QVector<CodeDropSlot *> fillSlots;
    auto filledBlocks = [&fillSlots]() {
        QStringList blocks;
        for (const CodeDropSlot *slot : fillSlots) {
            blocks << slot->property("blockId").toString();
        }
        return blocks;
    };

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
                    return codeForBlock(blockId).simplified();
                });
                fillSlots.append(slot);
                lineLayout->addWidget(slot, 0, Qt::AlignVCenter);
            } else {
                const bool unlocked = collectedClues.contains(tokenId);
                QLabel *clue = new QLabel(lineWidget);
                clue->setText(unlocked ? levels.at(currentLevelIndex).clues.value(tokenId).val.trimmed() : hiddenCodeMask());
                clue->setTextFormat(Qt::PlainText);
                clue->setStyleSheet(unlocked
                                        ? "QLabel { color: #a8f4bf; font-family: Consolas, monospace; font-size: 16px; background: rgba(10, 38, 47, 150); border-radius: 5px; padding: 2px 8px; }"
                                        : "QLabel { color: rgba(249, 241, 208, 80); font-family: Consolas, monospace; font-size: 16px; background: rgba(26, 29, 38, 210); border: 1px solid rgba(120, 130, 145, 120); border-radius: 5px; padding: 2px 8px; }");
                lineLayout->addWidget(clue, 0, Qt::AlignVCenter);
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
    connect(submitButton, &QPushButton::clicked, this, [this, &dialog, &filledBlocks, &wonBoss, monster, monsterId]() {
        const QStringList blocks = filledBlocks();
        if (blocks.size() != monster.spaces.size()) {
            QMessageBox::warning(&dialog, "Wrong Fill", QString("Need %1 block(s), got %2.").arg(monster.spaces.size()).arg(blocks.size()));
            return;
        }
        for (int i = 0; i < blocks.size(); ++i) {
            if (blocks.at(i).isEmpty()) {
                QMessageBox::warning(&dialog, "Wrong Fill", QString("Fill %1 first.").arg(monster.spaces.at(i).spaceId));
                return;
            }
            if (!bagBlocks.contains(blocks.at(i)) && monsterId != "boss") {
                QMessageBox::warning(&dialog, "Wrong Fill", QString("%1 is not in your bag.").arg(blocks.at(i)));
                return;
            }
            if (!blockMatchesSpace(blocks.at(i), monster.spaces.at(i))) {
                QMessageBox::warning(&dialog, "Wrong Fill", QString("%1 does not match %2.").arg(blocks.at(i), monster.spaces.at(i).spaceId));
                return;
            }
        }
        pushUndoState();
        defeatedMonsters.insert(monsterId);
        seenMonsters.insert(monsterId);
        if (monsterId == "boss") {
            completedStageIndexes.insert(currentLevelIndex);
            wonBoss = true;
        } else {
            for (const QString &blockId : blocks) {
                bagBlocks.removeOne(blockId);
            }
            const CodeBlock synthesized = synthesizeMonsterBlock(monster, blocks);
            if (!synthesized.blockId.isEmpty()) {
                knownCodeBlocks[synthesized.blockId] = synthesized;
                bagBlocks.append(synthesized.blockId);
            }
        }
        refreshGameUi();
        ui->combatLogLabel->setText(monsterId == "boss" ? levels.at(currentLevelIndex).endText : "Correct fill. Enemy defeated.");
        dialog.accept();
    });
    QHBoxLayout *combatTop = new QHBoxLayout();
    combatTop->addWidget(hero);
    combatTop->addWidget(codeScroll, 1);
    layout->addWidget(title);
    layout->addLayout(combatTop);

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
    for (const QString &blockId : bagBlocks) {
        CodeBlockIcon *blockIcon = new CodeBlockIcon(blockId, codeBlockIconPath(blockId), bagStrip);
        installHoverPopup(blockIcon,
                          QString("<b>%1</b><br>Type: %2<br><pre>%3</pre>")
                              .arg(blockId.toHtmlEscaped(),
                                   typeForBlock(blockId).toHtmlEscaped(),
                                   codeForBlock(blockId).toHtmlEscaped()));
        bagIconRow->addWidget(blockIcon);
    }
    bagIconRow->addStretch();
    bagScroll->setWidget(bagStrip);
    layout->addWidget(bagScroll);
    layout->addWidget(buttons);
    dialog.resize(860, 660);
    dialog.exec();
    returnToPreviousTile();
    if (wonBoss) {
        showVictorySettlement();
    }
}

void MainWindow::submitFill()
{
    const QString tileId = tileAt(playerRow, playerColumn);
    if (!(tileId.startsWith("monster") || tileId == "boss")) {
        ui->combatLogLabel->setText("Stand on a monster or boss tile first.");
        return;
    }
    if (defeatedMonsters.contains(tileId)) {
        ui->combatLogLabel->setText("Already defeated.");
        return;
    }

    const Monster monster = monsterByTile(tileId);
    const QStringList blocks = splitAnswerBlocks(ui->answerLineEdit->text());
    if (blocks.size() != monster.spaces.size()) {
        ui->combatLogLabel->setText(QString("Need %1 block(s), got %2.").arg(monster.spaces.size()).arg(blocks.size()));
        return;
    }

    for (int i = 0; i < blocks.size(); ++i) {
        if (!bagBlocks.contains(blocks.at(i)) && tileId != "boss") {
            ui->combatLogLabel->setText(QString("%1 is not in your bag.").arg(blocks.at(i)));
            return;
        }
        if (!blockMatchesSpace(blocks.at(i), monster.spaces.at(i))) {
            ui->combatLogLabel->setText(QString("%1 does not match %2.").arg(blocks.at(i), monster.spaces.at(i).spaceId));
            return;
        }
    }

    pushUndoState();
    defeatedMonsters.insert(tileId);
    seenMonsters.insert(tileId);
    if (tileId == "boss") {
        completedStageIndexes.insert(currentLevelIndex);
    } else {
        for (const QString &blockId : blocks) {
            bagBlocks.removeOne(blockId);
        }
        const CodeBlock synthesized = synthesizeMonsterBlock(monster, blocks);
        if (!synthesized.blockId.isEmpty()) {
            knownCodeBlocks[synthesized.blockId] = synthesized;
            bagBlocks.append(synthesized.blockId);
        }
    }
    ui->answerLineEdit->clear();
    ui->combatLogLabel->setText(tileId == "boss" ? levels.at(currentLevelIndex).endText : "Correct fill. Enemy defeated.");
    refreshGameUi();
    if (tileId == "boss") {
        showVictorySettlement();
    }
}

QString MainWindow::renderMonsterCode(const Monster &monster) const
{
    QString code = monster.codeTemplate;
    for (const Space &space : monster.spaces) {
        code.replace(QString("$%1$").arg(space.spaceId), "____________");
    }
    for (auto it = levels.at(currentLevelIndex).clues.constBegin(); it != levels.at(currentLevelIndex).clues.constEnd(); ++it) {
        code.replace(QString("$%1$").arg(it.key()), collectedClues.contains(it.key()) ? it.value().val : hiddenCodeMask());
    }
    return code;
}

QString MainWindow::renderMonsterCodeHtml(const Monster &monster) const
{
    QString html = monster.codeTemplate.toHtmlEscaped();
    html.replace('\t', "    ");
    for (const Space &space : monster.spaces) {
        const QString token = QString("$%1$").arg(space.spaceId).toHtmlEscaped();
        const QString cell = QString("<span style=\"%1\">              </span>")
                                 .arg(codeTokenStyle());
        html.replace(token, cell);
    }
    for (auto it = levels.at(currentLevelIndex).clues.constBegin(); it != levels.at(currentLevelIndex).clues.constEnd(); ++it) {
        const QString token = QString("$%1$").arg(it.key()).toHtmlEscaped();
        const bool unlocked = collectedClues.contains(it.key());
        QString value = unlocked ? it.value().val.toHtmlEscaped() : hiddenCodeMask();
        value.replace('\t', "    ");
        const QString style = unlocked
                                  ? codeTokenStyle()
                                  : QString("display:inline-block;padding:3px 8px;margin:0 3px;border:1px solid #5e6470;border-radius:5px;background:#1a1d26;color:#59606c;font-weight:700;");
        html.replace(token, QString("<span style=\"%1\">%2</span>").arg(style, value));
    }
    return QString(
               "<pre style=\"font-family:Consolas,'Cascadia Mono',monospace;"
               "font-size:15px;line-height:1.45;color:#f9f1d0;background:#111827;"
               "padding:14px;margin:0;white-space:pre-wrap;tab-size:4;\">%1</pre>")
        .arg(html);
}

QString MainWindow::codeForBlock(const QString &blockId) const
{
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

CodeBlock MainWindow::synthesizeMonsterBlock(const Monster &monster, const QStringList &blocks) const
{
    CodeBlock synthesized;
    if (blocks.size() != monster.spaces.size()) {
        return synthesized;
    }

    synthesized.blockId = monster.name.isEmpty() ? monster.monsterId : monster.name;
    synthesized.blockId += "[";
    for (int i = 0; i < blocks.size(); ++i) {
        synthesized.blockId += blocks.at(i);
        if (i + 1 < blocks.size()) {
            synthesized.blockId += ",";
        }
    }
    synthesized.blockId += "]";
    synthesized.type = monster.type;

    Synthesis synthesis = templateBreakdown(monster.codeTemplate);
    QMap<QString, QString> filledCode;
    for (int i = 0; i < monster.spaces.size(); ++i) {
        filledCode[monster.spaces.at(i).spaceId] = codeForBlock(blocks.at(i));
    }

    for (int i = 0; i < synthesis.text.length(); ++i) {
        synthesized.code += synthesis.text.at(i);
        if (i >= synthesis.cell.length()) {
            continue;
        }
        const SynthesisCell cell = synthesis.cell.at(i);
        if (cell.type == "clue") {
            synthesized.code += levels.at(currentLevelIndex).clues.value(cell.id).val;
        } else if (cell.type == "space") {
            synthesized.code += filledCode.value(cell.id);
        }
    }

    if (synthesized.code.isEmpty()) {
        synthesized.code = renderMonsterCode(monster);
    }
    return synthesized;
}

bool MainWindow::chestHasAvailableBlocks(const QString &chestId) const
{
    if (currentLevelIndex < 0 || currentLevelIndex >= levels.size()) {
        return false;
    }
    if (remainingChestBlocks.contains(chestId)) {
        return !remainingChestBlocks.value(chestId).isEmpty();
    }

    const Chest chest = levels.at(currentLevelIndex).chests.value(chestId);
    return !chest.blocks.isEmpty();
}

bool MainWindow::blockMatchesSpace(const QString &blockId, const Space &space) const
{
    for (const QString &rule : space.values) {
        if (space.type == "prefix" && blockId.startsWith(rule)) {
            return true;
        }
        if (space.type == "find" && blockId.contains(rule)) {
            return true;
        }
        if (space.type == "regex") {
            const QRegularExpression regex(rule);
            if (regex.isValid() && regex.match(blockId).hasMatch()) {
                return true;
            }
        }
        if ((space.type == "match" || space.type.isEmpty()) && blockId == rule) {
            return true;
        }
    }
    return false;
}

QStringList MainWindow::splitAnswerBlocks(const QString &text) const
{
    QString normalized = text;
    normalized.replace(';', ',');
    QStringList parts = normalized.split(',', Qt::SkipEmptyParts);
    for (QString &part : parts) {
        part = part.trimmed();
    }
    return parts;
}

Monster MainWindow::monsterByTile(const QString &tileId) const
{
    if (tileId == "boss") {
        return levels.at(currentLevelIndex).boss;
    }
    return levels.at(currentLevelIndex).monsters.value(tileId);
}
