#ifndef CODEBLOCKUI_H
#define CODEBLOCKUI_H

#include <QString>
#include <QWidget>
#include <QLabel>
#include <QToolButton>
#include <QObject>
#include <QLayout>
#include <QIcon>
#include <QPixmap>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QPoint>
#include <QSizePolicy>
#include <functional>

inline constexpr const char *codeBlockMimeType = "application/x-compile-spire-code-block";

// ---- code/clue text + HTML formatting helpers ----
QString codeTokenStyle();
QString hiddenCodeMask();
QString formatCodeBlockForDisplay(QString code);
QString indentContinuationLines(QString text, const QString &continuationIndent);
QString formatClueForCode(const QString &clue);
QString formatClueListLine(const QString &clueId, const QString &clueText);
QString replaceCodeTemplateTokens(const QString &codeTemplate, const std::function<QString(const QString &)> &replacementForToken);
QString codeBlockHtml(const QString &code);
QString codeBlockPopupHtml(const QString &code, const QString &type = QString());

// ---- generic UI helpers ----
void clearLayout(QLayout *layout);
void installHoverPopup(QWidget *widget, const QString &html);

// ---- draggable code-block icon / drop slot / hover popup ----
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
        setMinimumSize(220, 28);
        setMaximumWidth(520);
        setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        setWordWrap(true);
        setProperty("blockId", QString());
        setText(QString());
        setTextFormat(Qt::PlainText);
        refreshStyle(false);
    }

    void setOnChanged(std::function<void()> callback)
    {
        m_onChanged = std::move(callback);
    }

    void setOnRemoveRequested(std::function<void()> callback)
    {
        m_onRemoveRequested = std::move(callback);
    }

    void setTextProvider(std::function<QString(const QString &)> provider)
    {
        m_textProvider = std::move(provider);
    }

    void clearBlock()
    {
        setProperty("blockId", QString());
        setProperty("previousBlockId", QString());
        setText(QString());
        setWordWrap(false);
        setMinimumWidth(220);
        refreshStyle(false);
    }

    void setBlock(const QString &blockId, const QString &displayText)
    {
        setProperty("blockId", blockId);
        setText(displayText);
        const bool multiline = displayText.contains('\n');
        setWordWrap(multiline);
        QFont codeFont("Consolas");
        codeFont.setPointSize(16);
        codeFont.setBold(true);
        const QFontMetrics metrics(codeFont);
        int contentWidth = 0;
        for (const QString &line : displayText.split('\n')) {
            contentWidth = qMax(contentWidth, metrics.horizontalAdvance(line));
        }
        setMinimumWidth(multiline ? 260 : qMax(72, contentWidth + 10));
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

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton
            && !property("blockId").toString().isEmpty()
            && m_onRemoveRequested) {
            m_onRemoveRequested();
            event->accept();
            return;
        }
        QLabel::mouseDoubleClickEvent(event);
    }

private:
    void refreshStyle(bool hovered)
    {
        const bool filled = !property("blockId").toString().isEmpty();
        const QString border = hovered ? "#49e6ff" : (filled ? "#d7b75f" : "#526170");
        const QString background = filled ? "rgba(10, 38, 47, 225)" : "rgba(12, 18, 26, 215)";
        const QString padding = filled ? "2px 2px" : "2px 10px";
        setStyleSheet(QString(
            "QLabel { color: %1; background: %2; border: 1px solid %3; border-radius: 6px;"
            "font-family: Consolas, 'Microsoft YaHei UI'; font-size: 16px; font-weight: 700; padding: %4; }"
        ).arg(filled ? "#f9f1d0" : "#6f7f8b", background, border, padding));
    }

    std::function<void()> m_onChanged;
    std::function<void()> m_onRemoveRequested;
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

#endif // CODEBLOCKUI_H
