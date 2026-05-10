#pragma once

/**
 * @file AttentionPanel.h
 * @brief Collapsible attention event drawer displayed above the tab bar.
 */

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

/**
 * @brief Collapsible drawer that displays attention events from all characters.
 *
 * Sits above the QTabWidget in MainWindow. When collapsed, shows a badge
 * "[!] N issues". When expanded, shows a scrollable list of entries, each
 * displaying:
 * - Character identifier
 * - Event description
 * - A Dismiss button
 *
 * Pressing Dismiss emits dismissRequested(), which MainWindow forwards as
 * a MSG_CLEAR_ATTENTION frame to the daemon.
 *
 * The panel expands automatically when the first event is added and
 * collapses automatically when the last event is dismissed.
 *
 * @see MainWindow, MSG_ATTENTION_EVENT, MSG_CLEAR_ATTENTION
 */
class AttentionPanel : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Construct the panel.
     * @param parent Qt parent widget.
     */
    explicit AttentionPanel(QWidget *parent = nullptr);

    /**
     * @brief Add an attention event entry to the panel.
     *
     * The panel expands automatically if this is the first event.
     *
     * @param characterId  Character that raised the event.
     * @param eventId      Unique event ID (from the daemon).
     * @param eventType    Event type byte (from MSG_ATTENTION_EVENT payload).
     * @param text         Human-readable event description.
     */
    void addEvent(quint8 characterId, quint16 eventId, quint8 eventType,
                  const QString &text);

    /**
     * @brief Remove a specific event from the panel.
     *
     * Called after the daemon acknowledges a ClearAttention request via
     * MSG_ATTENTION_CLEARED.
     *
     * @param characterId  Owning character.
     * @param eventId      Event to remove.
     */
    void removeEvent(quint8 characterId, quint16 eventId);

    /**
     * @brief Remove all events belonging to a character.
     *
     * Called when a character disconnects or its session ends.
     *
     * @param characterId  Character whose events should be cleared.
     */
    void clearCharacter(quint8 characterId);

    /// @return Total number of unresolved attention events currently shown.
    int eventCount() const { return m_eventCount; }

signals:
    /**
     * @brief Emitted when the user presses a Dismiss button.
     *
     * MainWindow handles this by sending MSG_CLEAR_ATTENTION to the daemon.
     *
     * @param characterId  Character that owns the event.
     * @param eventId      Event to dismiss.
     */
    void dismissRequested(quint8 characterId, quint16 eventId);

private slots:
    void toggleExpanded();
    void updateBadge();

private:
    void setExpanded(bool expanded);

    QWidget     *m_drawer       = nullptr; ///< Collapsible container for event rows.
    QVBoxLayout *m_drawerLayout = nullptr; ///< Layout inside the drawer.
    QPushButton *m_toggleButton = nullptr; ///< Always-visible collapse/expand button.
    int          m_eventCount   = 0;       ///< Current number of displayed events.
    bool         m_expanded     = false;   ///< Whether the drawer is currently visible.
};
