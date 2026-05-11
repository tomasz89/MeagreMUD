#pragma once

/**
 * @file SessionLogger.h
 * @brief Appends stripped plain-text session output to a log file.
 *
 * Lives on the CharacterSession thread.
 */

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

/**
 * @brief Writes stripped MUD output to a plain-text session log file.
 *
 * Receives lines from AnsiStripper::strippedText() and appends them to
 * the log file one line at a time. The log path is derived from the
 * character ID and session start time and recorded in the SessionLog
 * database table by CharacterSession.
 *
 * ## File handling
 * - The file is opened lazily on the first line received.
 * - It is kept open for the duration of the session and closed in stop().
 * - Each line is written with a timestamp prefix if timestamping is enabled.
 * - The file is flushed after every write to ensure log completeness if
 *   the daemon crashes.
 *
 * ## Log format (without timestamp)
 * @code
 * You enter the room.
 * A goblin attacks you!
 * [HP:80 MA:50] >
 * @endcode
 *
 * ## Log format (with timestamp)
 * @code
 * [12:34:56] You enter the room.
 * [12:34:56] A goblin attacks you!
 * @endcode
 *
 * @see AnsiStripper, CharacterSession
 */
class SessionLogger : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct the logger.
     * @param parent Qt parent object.
     */
    explicit SessionLogger(QObject *parent = nullptr);

    /**
     * @brief Destructor — closes the log file if open.
     */
    ~SessionLogger() override;

    /**
     * @brief Configure the log file path and options.
     *
     * Must be called before the first line is received. Safe to call
     * multiple times; reopens the file if already open.
     *
     * @param logPath        Full path to the log file.
     * @param timestampLines If true, each line is prefixed with HH:mm:ss.
     */
    void configure(const QString &logPath, bool timestampLines = false);

    /**
     * @brief Close the log file and release the file handle.
     *
     * Called by CharacterSession on session end. Safe to call if the
     * file was never opened.
     */
    void stop();

    /// @return The configured log file path, or an empty string if not set.
    QString logPath() const { return m_logPath; }

    /// @return true if the log file is currently open and being written to.
    bool isOpen() const { return m_file.isOpen(); }

public slots:
    /**
     * @brief Receive a plain-text line and append it to the log file.
     *
     * Connected from AnsiStripper::strippedText(). Opens the file on
     * first call if not already open.
     *
     * @param text  Plain-text line (no trailing newline).
     */
    void onStrippedText(const QString &text);

private:
    /**
     * @brief Open the log file for appending.
     * @return true if the file was opened successfully.
     */
    bool openFile();

    QString      m_logPath;
    bool         m_timestampLines = false;
    QFile        m_file;
    QTextStream  m_stream;
};
