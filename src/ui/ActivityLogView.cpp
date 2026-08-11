#include "ActivityLogView.h"

#include <QTextEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QTimer>
#include <QTextCursor>

namespace tagit {

ActivityLogView::ActivityLogView(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

void ActivityLogView::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_text = new QTextEdit(this);
    m_text->setReadOnly(true);
    m_text->document()->setMaximumBlockCount(2000);
    m_text->setLineWrapMode(QTextEdit::NoWrap);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    m_clearButton = new QPushButton(tr("Clear"), this);
    buttonRow->addWidget(m_clearButton);

    layout->addWidget(m_text);
    layout->addLayout(buttonRow);

    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(50);
    m_flushTimer->setSingleShot(true);
    connect(m_flushTimer, &QTimer::timeout, this, &ActivityLogView::flushLog);

    connect(m_clearButton, &QPushButton::clicked, this, &ActivityLogView::clearLog);
}

void ActivityLogView::appendEntry(const QString &entry)
{
    m_buffer.append(entry);
    if (!m_flushTimer->isActive()) {
        m_flushTimer->start();
    }
}

void ActivityLogView::flushLog()
{
    if (m_buffer.isEmpty()) return;

    QString batch = m_buffer.join(QLatin1Char('\n'));
    m_buffer.clear();

    QTextCursor cursor = m_text->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (m_text->document()->blockCount() > 1 || !m_text->toPlainText().isEmpty()) {
        cursor.insertText(QLatin1String("\n") + batch);
    } else {
        cursor.insertText(batch);
    }
    m_text->setTextCursor(cursor);

    QScrollBar *sb = m_text->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
}

void ActivityLogView::setEntries(const QStringList &lines)
{
    m_buffer.clear();
    if (m_flushTimer && m_flushTimer->isActive()) {
        m_flushTimer->stop();
    }
    m_text->clear();
    if (!lines.isEmpty()) {
        m_text->setPlainText(lines.join(QLatin1Char('\n')));
    }
}

void ActivityLogView::clearLog()
{
    m_buffer.clear();
    if (m_flushTimer && m_flushTimer->isActive()) {
        m_flushTimer->stop();
    }
    m_text->clear();
}

} // namespace tagit

