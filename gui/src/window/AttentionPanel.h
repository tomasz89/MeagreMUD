#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

// ---------------------------------------------------------------------------
// AttentionPanel
//
// Collapsible drawer displayed above the tab bar. Shows attention events
// from all characters. Collapsed state shows "⚠ N issues" badge.
// Expanded state shows a scrollable list of entries, each with:
//   character name | timestamp | type | description | [Dismiss] button
//
// Dismiss sends MSG_CLEAR_ATTENTION (0x43) via the connection manager.
// ---------------------------------------------------------------------------

class AttentionPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AttentionPanel(QWidget *parent = nullptr);

    // Add an attention event entry.
    void addEvent(quint8 characterId, quint16 eventId, quint8 eventType,
                  const QString &text);

    // Remove a specific event (after ClearAttention is acknowledged).
    void removeEvent(quint8 characterId, quint16 eventId);

    // Clear all events for a character (e.g. on disconnect).
    void clearCharacter(quint8 characterId);

    int eventCount() const { return m_eventCount; }

signals:
    // Emitted when the user presses Dismiss on an entry.
    void dismissRequested(quint8 characterId, quint16 eventId);

private slots:
    void toggleExpanded();
    void updateBadge();

private:
    void setExpanded(bool expanded);

    QWidget     *m_drawer       = nullptr;
    QVBoxLayout *m_drawerLayout = nullptr;
    QPushButton *m_toggleButton = nullptr;
    int          m_eventCount   = 0;
    bool         m_expanded     = false;
};
