#include "codeblockui.h"

#include <QRegularExpression>
#include <QStringList>

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

QString codeBlockPopupHtml(const QString &code, const QString &type)
{
    const QString typeLine = type.isEmpty()
                                 ? QString()
                                 : QString("Type: %1<br>").arg(type.toHtmlEscaped());
    return QString("%1<pre>%2</pre>").arg(typeLine, codeBlockHtml(code));
}

void installHoverPopup(QWidget *widget, const QString &html)
{
    widget->installEventFilter(new HoverPopupFilter(widget, html));
    widget->setMouseTracking(true);
}
