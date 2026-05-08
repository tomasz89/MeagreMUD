#pragma once

/**
 * @file CharacterPane.h
 * @brief Composite per-character UI pane: terminal + status bar + input.
 */

#include <QWidget>
#include <QString>
#include "protocol/Protocol.h"
#include "terminal/TerminalWidget.h"
#include "pane/StatusLine.h"
#include "input/InputWidget.h"

/**
 * @brief Composite widget representing one character's complete UI surface.
 *
 * Contains a TerminalWidget (top, stretches), StatusLine (fixed height),
 * and InputWidget (fixed height) stacked vertically. The same CharacterPane
 * instance is reparented when the user docks or undocks a character between
 * the QTabWidget and TiledArea — no state is lost on reparenting.
 *
 * ## Observer mode
 * When the GUI is an observer rather than the controller, input is locked
 * but the terminal display and status bar are unaffected. The status bar's
 * connection status label indicates observer state.
 *
 * ## Input routing
 * User-submitted text is emitted as inputSubmitted() and forwarded by
 * MainWindow to DaemonConnectionManager::sendFrame().
 *
 * @see TerminalWidget, StatusLine, InputWidget, MainWindow
 */
class CharacterPane : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Construct the pane.
     * @param characterId        Daemon character ID (1–16).
     * @param characterName      Display name shown in the status bar and tab.
     * @param backbufferCapacity Lines to retain in the terminal backbuffer.
     * @param parent             Qt parent widget.
     */
    explicit CharacterPane(quint8 characterId, const QString &characterName,
                           int backbufferCapacity = TerminalWidget::DEFAULT_BACKBUFFER_LINES,
                           QWidget *parent = nullptr);

    /// @return The daemon character ID for this pane.
    quint8  characterId()   const { return m_characterId; }

    /// @return The character's display name.
    QString characterName() const { return m_characterName; }

    /// @return The current MUD connection status.
    CharacterStatus status() const { return m_status; }

    /**
     * @brief Update the MUD connection status shown in the status bar.
     * @param status  New character connection status.
     */
    void setCharacterStatus(CharacterStatus status);

    /**
     * @brief Append a StyledRun to the terminal display.
     * @param run  Styled text run received from the daemon.
     */
    void appendRun(const StyledRun &run);

    /**
     * @brief Clear the terminal display (ESC[2J received from daemon).
     */
    void clearScreen();

    /**
     * @brief Update the engine mode shown in the status bar.
     * @param mode  New engine operating mode.
     */
    void setEngineMode(EngineMode mode);

    /**
     * @brief Update the combat flag shown in the status bar.
     * @param flag  New combat flag.
     */
    void setCombatFlag(CombatFlag flag);

    /**
     * @brief Update the room name shown in the status bar.
     * @param roomName  Current room name; empty shows "(unknown room)".
     */
    void setRoomName(const QString &roomName);

    /**
     * @brief Update the script running indicator in the status bar.
     * @param running @c true to show the indicator.
     */
    void setScriptRunning(bool running);

    /**
     * @brief Update the server name shown in the status bar.
     * @param serverName  BBS or server display name.
     */
    void setServerName(const QString &serverName);

    /**
     * @brief Lock or unlock the input widget.
     *
     * Called by MainWindow during resync (locked) and on connect (unlocked).
     *
     * @param locked @c true to lock input.
     */
    void setInputLocked(bool locked);

    /// @return @c true if input is currently locked.
    bool isInputLocked() const { return m_inputLocked; }

    /**
     * @brief Enable or disable observer mode.
     *
     * Observer mode locks input but does not affect the terminal or status bar.
     *
     * @param observer @c true to enter observer mode.
     */
    void setObserverMode(bool observer);

    /**
     * @brief Register a key macro on the input widget.
     * @param key        Qt::Key value.
     * @param modifiers  Modifier bitmask.
     * @param macroText  Text to submit on activation.
     */
    void addMacro(int key, Qt::KeyboardModifiers modifiers,
                  const QString &macroText);

    /// Remove all registered macros from the input widget.
    void clearMacros();

    /// @return The embedded TerminalWidget. Never null after construction.
    TerminalWidget *terminalWidget() const { return m_terminal; }

    /// @return The embedded StatusLine. Never null after construction.
    StatusLine     *statusLine()     const { return m_statusLine; }

    /// @return The embedded InputWidget. Never null after construction.
    InputWidget    *inputWidget()    const { return m_inputWidget; }

signals:
    /**
     * @brief Emitted when the user submits text via the input widget.
     *
     * Text already has @c \\r\\n appended. MainWindow forwards this to
     * DaemonConnectionManager::sendFrame().
     *
     * @param characterId  This pane's character ID.
     * @param text         Submitted text including trailing \\r\\n.
     */
    void inputSubmitted(quint8 characterId, const QString &text);

private slots:
    void onTextSubmitted(const QString &text);

private:
    quint8          m_characterId;
    QString         m_characterName;
    CharacterStatus m_status        = CharacterStatus::Disconnected;
    bool            m_observerMode  = false;
    bool            m_inputLocked   = false;

    TerminalWidget *m_terminal    = nullptr;
    StatusLine     *m_statusLine  = nullptr;
    InputWidget    *m_inputWidget = nullptr;
};
