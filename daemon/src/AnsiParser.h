#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QVector>
#include "types/MudTypes.h"

/**
 * @file AnsiParser.h
 * @brief ANSI escape sequence parser for the MUD byte stream.
 */

/**
 * @brief Parses a raw MUD TCP byte stream into StyledRun objects.
 *
 * Lives on the CharacterSession thread. Processes bytes fed via onData()
 * and emits runReady() whenever an ANSI SGR escape sequence completes,
 * bundling all text accumulated since the previous style change.
 *
 * @par Core rule — no newline flush
 * runReady() is emitted on escape-sequence completion, @b never on @c \\n.
 * MUD prompts sit on open lines indefinitely with no @c \\n arriving; any
 * design that waits for @c \\n will stall the display. @c \\n appears inside
 * run text as a normal character and is handled by TerminalWidget.
 *
 * @par State machine
 * @code
 * Normal      → 0x1B → EscapeStart
 *             → \\r  → set pendingCR
 *             → other → append to run text
 * EscapeStart → '['  → CSI
 *             → other → drop ESC, re-process char
 * CSI         → digit/';' → CSIParams
 *             → 'H'  → drop (cursor home)
 *             → 'm'  → implicit SGR reset, back to Normal
 *             → '2'  → CSIErase
 *             → other → drop
 * CSIParams   → digit/';' → accumulate
 *             → 'm'  → parse SGR, emit run, apply style, back to Normal
 *             → other → drop sequence
 * CSIErase    → 'J'  → emit screenCleared(), back to Normal
 *             → other → drop
 * @endcode
 *
 * @par pendingCR behaviour
 * When @c \\r is seen: set @c pendingCR. Next character:
 * - @c \\n → append @c \\r\\n to run text, clear pendingCR.
 * - other → append @c \\r to run text, clear pendingCR, process the character.
 *
 * @par SGR codes handled
 * - @c 0        — reset all attributes
 * - @c 1        — bold on
 * - @c 30–37   — foreground normal colours (palette indices 0–7)
 * - @c 39       — foreground default
 * - @c 40–47   — background normal colours (palette indices 0–7)
 * - @c 49       — background default
 * - @c 90–97   — foreground bright colours (palette indices 8–15)
 * - @c 100–107 — background bright colours (palette indices 8–15)
 *
 * All other escape sequences are dropped silently.
 *
 * @note SGR is server→client only. Client input is never parsed here.
 */
class AnsiParser : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the parser in the Normal state with default style.
     * @param parent Optional Qt parent object.
     */
    explicit AnsiParser(QObject *parent = nullptr);

    /**
     * @brief Feeds raw bytes from the MUD socket into the parser.
     *
     * May emit runReady() zero or more times, and screenCleared() at most
     * once per ESC[2J sequence encountered in @p data.
     *
     * @param data Raw bytes received from the MUD TCP socket.
     */
    void onData(const QByteArray &data);

    /**
     * @brief Resets all parser state to the initial condition.
     *
     * Call when the MUD connection drops and reconnects to discard any
     * partial escape sequence or accumulated run text.
     */
    void reset();

signals:
    /**
     * @brief Emitted when an ANSI SGR escape sequence completes.
     *
     * The run contains all text accumulated since the previous style change,
     * tagged with the style that was active for that text. Never emitted
     * with an empty text field.
     *
     * @param run The completed styled run.
     */
    void runReady(StyledRun run);

    /**
     * @brief Emitted when ESC[2J (erase display) is received.
     *
     * The CharacterSession forwards this to DaemonServer, which can
     * broadcast a screen-clear notification to all connected GUIs.
     */
    void screenCleared();

private:
    /**
     * @brief Parser state machine states.
     */
    enum class State {
        Normal,       ///< Accumulating plain text.
        EscapeStart,  ///< Received 0x1B; waiting for '['.
        CSI,          ///< Received ESC[; waiting for parameters or final byte.
        CSIParams,    ///< Accumulating digit/semicolon parameter bytes.
        CSIErase,     ///< Received ESC[2; waiting for 'J'.
    };

    /**
     * @brief Processes a single byte through the state machine.
     * @param ch The byte to process.
     */
    void processChar(char ch);

    /**
     * @brief Emits runReady() with accumulated text and current run style.
     *
     * No-op if @c m_runText is empty.
     */
    void flushRun();

    /**
     * @brief Applies a parsed set of SGR codes to the current style.
     *
     * Calls flushRun() first to emit any accumulated text under the old style,
     * then updates @c m_fg, @c m_bg, @c m_bold, and the run-start style
     * snapshot.
     *
     * @param params Parsed integer SGR parameter list (may contain multiple
     *               codes separated by ';' in the original sequence).
     */
    void applySGR(const QVector<int> &params);

    /**
     * @brief Parses @c m_paramBuf into a list of integer SGR codes.
     *
     * An empty buffer is treated as @c {0} (implicit reset). Empty fields
     * between semicolons are treated as @c 0.
     *
     * @return Vector of parsed integer SGR codes.
     */
    QVector<int> parseParams() const;

    State   m_state     = State::Normal; ///< Current state machine state.
    bool    m_pendingCR = false;         ///< True if \\r was seen without a following \\n.

    QString m_runText;  ///< Text accumulated for the current run.

    /// @name Current active style (updated by SGR codes)
    /// @{
    quint8  m_fg   = COLOUR_DEFAULT;
    quint8  m_bg   = COLOUR_DEFAULT;
    bool    m_bold = false;
    /// @}

    /// @name Style snapshot at the start of the current run (emitted with the run)
    /// @{
    quint8  m_runFg   = COLOUR_DEFAULT;
    quint8  m_runBg   = COLOUR_DEFAULT;
    bool    m_runBold = false;
    /// @}

    QString m_paramBuf; ///< Accumulation buffer for CSI parameter bytes.
};
