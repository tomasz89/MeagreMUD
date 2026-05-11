#pragma once

/**
 * @file AnsiStripper.h
 * @brief Strips ANSI style information from the StyledRun stream, buffering
 *        output to newline boundaries for trigger granularity.
 *
 * Lives on the CharacterSession thread alongside AnsiParser.
 */

#include <QObject>
#include <QString>
#include "types/MudTypes.h"

/**
 * @brief Converts the StyledRun stream into plain-text lines for the
 *        script engine, path engine, and session logger.
 *
 * ## Design
 * AnsiParser deliberately never flushes on @c \\n — it flushes on escape
 * sequence completion so that MUD prompts (which have no trailing @c \\n)
 * are delivered immediately to the GUI.
 *
 * AnsiStripper takes the opposite approach for its consumers: it
 * accumulates stripped text and emits strippedText() only when a complete
 * line is available (i.e. when @c \\n is encountered). This gives the
 * ScriptEngine and PathEngine the line-granular input they need for pattern
 * matching without requiring them to buffer themselves.
 *
 * ## Pending text
 * Text that has not yet been terminated by @c \\n accumulates in an internal
 * buffer. It is flushed as a partial line when flush() is called (e.g. on
 * MUD disconnect) so that prompts and partial lines are not silently dropped.
 *
 * ## CR handling
 * @c \\r is stripped. @c \\r\\n is treated as a single newline.
 *
 * @see AnsiParser, ScriptEngine, PathEngine, SessionLogger
 */
class AnsiStripper : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct the stripper.
     * @param parent Qt parent object.
     */
    explicit AnsiStripper(QObject *parent = nullptr);

    /**
     * @brief Flush any buffered incomplete line as a partial strippedText().
     *
     * Called when the MUD connection drops so that partial prompt text
     * is not silently discarded. No-op if the buffer is empty.
     */
    void flush();

    /**
     * @brief Reset internal buffer state.
     *
     * Called when the MUD connection is re-established.
     */
    void reset();

public slots:
    /**
     * @brief Receive a StyledRun from AnsiParser and process its text.
     *
     * Strips style information, splits on @c \\n, and emits strippedText()
     * for each complete line. Incomplete lines are buffered.
     *
     * @param run  Styled text run from AnsiParser.
     */
    void onRunReady(StyledRun run);

signals:
    /**
     * @brief Emitted for each complete line of plain text.
     *
     * The newline character is NOT included in the emitted string.
     * Connected to ScriptEngine::onStrippedText(), PathEngine::onStrippedText(),
     * and SessionLogger::onStrippedText().
     *
     * @param text  Plain text line with no trailing newline.
     */
    void strippedText(QString text);

private:
    /// Pending text that has not yet been terminated by newline.
    QString m_buffer;
};
