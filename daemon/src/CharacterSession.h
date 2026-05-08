#pragma once

#include <QObject>
#include <QThread>
#include <QTcpSocket>
#include <QString>
#include "protocol/Protocol.h"
#include "types/MudTypes.h"
#include "AnsiParser.h"

/**
 * @file CharacterSession.h
 * @brief Per-character MUD connection manager running on its own thread.
 */

/**
 * @brief Manages one character's connection to the MUD server.
 *
 * Each CharacterSession runs on a dedicated QThread, owned by
 * CharacterRegistry. The MUD-side QTcpSocket is created in init() — never
 * in the constructor — to ensure it belongs to the session thread.
 *
 * @par Lifecycle
 * -# Main thread creates CharacterSession and a QThread.
 * -# CharacterSession is moved to the thread (@c moveToThread).
 * -# @c QThread::started → @c init() via queued connection.
 * -# @c init() creates the QTcpSocket and AnsiParser on the session thread.
 * -# Normal operation: MUD bytes → AnsiParser → StyledRun → runReady signal.
 * -# @c stop() sets a flag and disconnects the MUD socket.
 * -# @c onMudDisconnected() emits @c stopped() when the stop is clean.
 * -# CharacterRegistry calls @c QThread::quit() on receiving @c stopped().
 *
 * @par Cross-thread signals
 * All signals crossing the thread boundary (runReady, statusChanged,
 * roomIdentified, stopped) use Qt automatic queued connections established
 * by CharacterRegistry.
 *
 * @warning Never delete a CharacterSession directly. Call stop() and wait
 *          for the stopped() signal, then let QThread::finished →
 *          QObject::deleteLater handle cleanup.
 */
class CharacterSession : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the session. Does not create the MUD socket.
     * @param characterId   Unique 1-based identifier (1–16).
     * @param characterName Display name for log messages and the GUI.
     * @param parent        Optional Qt parent (normally nullptr; session is
     *                      moved to its own thread).
     */
    explicit CharacterSession(quint8 characterId,
                              const QString &characterName,
                              QObject *parent = nullptr);

    /// @brief Destructor. The MUD socket is parented and deleted automatically.
    ~CharacterSession() override;

    /// @brief Returns the character's unique identifier.
    quint8 characterId() const { return m_characterId; }

    /// @brief Returns the character's display name.
    QString characterName() const { return m_characterName; }

    /// @brief Returns the current MUD connection status.
    CharacterStatus status() const { return m_status; }

    /**
     * @brief Requests a clean shutdown of the session.
     *
     * Thread-safe — may be called from the main thread. Sets @c m_stopRequested
     * and disconnects the MUD socket. The session emits stopped() once the
     * socket has closed.
     */
    void stop();

signals:
    /**
     * @brief Emitted when a StyledRun is ready to broadcast to all GUIs.
     *
     * Connected with Qt::QueuedConnection by CharacterRegistry so it crosses
     * safely from the session thread to the main thread.
     *
     * @param characterId The character this run belongs to.
     * @param run         The parsed styled run.
     */
    void runReady(quint8 characterId, StyledRun run);

    /**
     * @brief Emitted when the character's MUD connection status changes.
     * @param characterId The character whose status changed.
     * @param status      The new status.
     */
    void statusChanged(quint8 characterId, CharacterStatus status);

    /**
     * @brief Emitted when a room has been identified from the MUD output.
     * @param characterId Owning character.
     * @param roomId      Database room ID (0 if not yet in DB).
     * @param roomName    Room name as received from the MUD.
     */
    void roomIdentified(quint8 characterId, quint16 roomId,
                        const QString &roomName);

    /**
     * @brief Emitted when the session thread has fully stopped.
     *
     * CharacterRegistry calls QThread::quit() on receiving this signal.
     *
     * @param characterId The character whose session stopped.
     */
    void stopped(quint8 characterId);

public slots:
    /**
     * @brief Initialises the session on the session thread.
     *
     * Called via queued connection from QThread::started. Creates the
     * QTcpSocket and AnsiParser here — never in the constructor — so that
     * both objects belong to the session thread.
     */
    void init();

    /**
     * @brief Sends text to the MUD server.
     *
     * Called from the main thread via QMetaObject::invokeMethod with
     * Qt::QueuedConnection. Text is written directly to the MUD socket.
     * No-op if not connected.
     *
     * @param text Text to send (should be @c \\r\\n terminated).
     */
    void sendInput(const QString &text);

private slots:
    /// @brief Called when the MUD TCP connection is established.
    void onMudConnected();

    /// @brief Called when the MUD TCP connection is lost.
    void onMudDisconnected();

    /**
     * @brief Called on MUD socket error.
     * @param error The socket error code.
     */
    void onMudError(QAbstractSocket::SocketError error);

    /// @brief Called when MUD data is available; feeds bytes to AnsiParser.
    void onMudReadyRead();

    /**
     * @brief Receives a StyledRun from AnsiParser and re-emits it with characterId.
     * @param run The parsed styled run.
     */
    void onRunReady(StyledRun run);

    /// @brief Handles ESC[2J from AnsiParser (screen clear).
    void onScreenCleared();

private:
    /**
     * @brief Updates the connection status and emits statusChanged().
     * @param status The new status to apply.
     */
    void setStatus(CharacterStatus status);

    /// @brief Initiates the MUD TCP connection using stored host/port.
    void connectToMud();

    quint8          m_characterId;   ///< Unique character identifier.
    QString         m_characterName; ///< Display name.
    CharacterStatus m_status = CharacterStatus::Disconnected; ///< Current status.

    AnsiParser     *m_ansiParser = nullptr; ///< ANSI parser (created in init()).

    /// MUD-side socket — created in init(), owned by this object's thread.
    QTcpSocket     *m_mudSocket = nullptr;

    QString         m_mudHost;       ///< MUD server hostname (loaded from DB).
    quint16         m_mudPort = 0;   ///< MUD server port (loaded from DB).

    bool            m_stopRequested = false; ///< Set by stop() to signal clean shutdown.
};
