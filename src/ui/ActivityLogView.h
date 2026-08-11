#ifndef TAGIT_ACTIVITY_LOG_VIEW_H
#define TAGIT_ACTIVITY_LOG_VIEW_H

#include <QWidget>
#include <QStringList>

class QTextEdit;
class QPushButton;

namespace tagit {

/**
 * @brief Scrollable, filterable view of recent application activity.
 *
 * Displays Logger entries in a read-only text area with a "Clear" button.
 */
class ActivityLogView : public QWidget {
    Q_OBJECT
public:
    explicit ActivityLogView(QWidget *parent = nullptr);

    /// Append a single formatted log line (buffered for smooth UI rendering).
    void appendEntry(const QString &entry);

    /// Force immediate flush of any buffered log entries.
    void flushLog();

    /// Replace the entire log contents with @p lines.
    void setEntries(const QStringList &lines);

    /// Clear the visible log.
    void clearLog();

private:
    void buildUi();

    QTextEdit *m_text = nullptr;
    QPushButton *m_clearButton = nullptr;
    QStringList m_buffer;
    class QTimer *m_flushTimer = nullptr;
};

} // namespace tagit

#endif // TAGIT_ACTIVITY_LOG_VIEW_H

