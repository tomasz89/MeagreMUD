#pragma once

/**
 * @file InputWidget.h
 * @brief Single-line command input bar with history and macro support.
 */

#include <QWidget>
#include <QLineEdit>
#include <QStringList>

/**
 * @brief Single-line text input widget for a character pane.
 *
 * Sits below the TerminalWidget in each CharacterPane. Handles Return/Enter
 * submission, command history navigation, and key macro interception.
 *
 * ## Behaviour
 * - Return or Enter submits the current text, appends @c \\r\\n, adds it to
 *   history, and clears the field.
 * - Up/Down arrow keys navigate the command history ring (up to
 *   HISTORY_SIZE entries). The current draft is saved when browsing begins
 *   and restored when the user returns to the bottom.
 * - Consecutive identical entries are deduplicated in the history.
 * - While locked (resync or observer mode) the field is visually disabled
 *   and ignores Return/Enter.
 * - Key macros are checked before any other processing and fire regardless
 *   of lock state. Macro text is submitted directly without touching the
 *   input field or history.
 *
 * @see CharacterPane, DaemonConnectionManager
 */
class InputWidget : public QWidget
{
    Q_OBJECT

public:
    /// Maximum number of entries retained in the command history ring.
    static constexpr int HISTORY_SIZE = 100;

    /**
     * @brief Construct the input widget.
     * @param parent Qt parent widget.
     */
    explicit InputWidget(QWidget *parent = nullptr);

    /**
     * @brief Lock or unlock the input field.
     *
     * When locked, the QLineEdit is disabled and placeholder text changes
     * to "(locked)". Key macros still fire when locked.
     *
     * @param locked @c true to lock; @c false to unlock.
     */
    void setLocked(bool locked);

    /// @return @c true if the input is currently locked.
    bool isLocked() const { return m_locked; }

    /**
     * @brief Register a key macro.
     *
     * When the specified key combination is pressed, @p macroText is
     * submitted immediately via textSubmitted() without affecting the input
     * field or history.
     *
     * @param key        Qt::Key value.
     * @param modifiers  Qt::KeyboardModifiers bitmask.
     * @param macroText  Text to submit (should already include \\r\\n if needed;
     *                   \\r\\n is appended automatically by submitText()).
     */
    void addMacro(int key, Qt::KeyboardModifiers modifiers,
                  const QString &macroText);

    /// Remove all registered key macros.
    void clearMacros();

    /**
     * @brief Programmatically set the input field text.
     *
     * Used to paste content (e.g. from the attention panel) without
     * submitting it. Focus is moved to the input widget.
     *
     * @param text  Text to place in the field.
     */
    void setText(const QString &text);

signals:
    /**
     * @brief Emitted when text is submitted by the user or a macro fires.
     *
     * @c \\r\\n is always appended before emission.
     *
     * @param text  The submitted text including trailing \\r\\n.
     */
    void textSubmitted(const QString &text);

private slots:
    void onReturnPressed();

protected:
    /// @cond INTERNAL
    bool eventFilter(QObject *watched, QEvent *event) override;
    /// @endcond

private:
    /// Internal representation of a registered key macro.
    struct Macro {
        int                   key;       ///< Qt::Key value.
        Qt::KeyboardModifiers modifiers; ///< Required modifier bitmask.
        QString               text;      ///< Text to submit on activation.
    };

    void historyUp();
    void historyDown();
    void submitText(const QString &text);

    QLineEdit    *m_edit         = nullptr;
    bool          m_locked       = false;

    QStringList   m_history;             ///< Command history ring (oldest first).
    int           m_historyIndex = -1;   ///< -1 = not currently browsing history.
    QString       m_draft;               ///< Draft text saved when browsing begins.

    QVector<Macro> m_macros;             ///< Registered key macros.
};
