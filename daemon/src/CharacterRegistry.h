#pragma once

#include <QObject>
#include <QMap>
#include <QThread>
#include "CharacterSession.h"

/**
 * @file CharacterRegistry.h
 * @brief Owns and manages all CharacterSession objects and their threads.
 */

/**
 * @brief Central registry for all active character sessions.
 *
 * Lives on the main thread. Creates CharacterSession objects, moves each to
 * its own QThread, and routes signals between sessions and DaemonServer.
 *
 * @par Threading
 * - CharacterRegistry itself lives entirely on the main thread.
 * - Each CharacterSession runs on its own QThread (one thread per character).
 * - Cross-thread signal/slot connections are established automatically
 *   (Qt::AutoConnection resolves to Qt::QueuedConnection across threads).
 *
 * @par Session lifecycle
 * addSession() creates and starts the session. stopAll() requests a clean
 * shutdown of all sessions and emits allStopped() once every session has
 * confirmed it is finished. CharacterRegistry::~CharacterRegistry() forces
 * any remaining threads to quit with a 3-second timeout.
 */
class CharacterRegistry : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an empty registry.
     * @param parent Optional Qt parent object.
     */
    explicit CharacterRegistry(QObject *parent = nullptr);

    /**
     * @brief Destructor. Forces all running session threads to quit.
     *
     * Prefer calling stopAll() before destruction to allow sessions to
     * shut down cleanly.
     */
    ~CharacterRegistry() override;

    /**
     * @brief Creates a new CharacterSession, starts its thread, and registers it.
     *
     * If a session with @p characterId already exists, returns the existing
     * session with a warning rather than creating a duplicate.
     *
     * @param characterId   Unique 1-based identifier (1 - 16).
     * @param characterName Display name for the GUI and logs.
     * @return Pointer to the session (owned by the registry). Never null.
     */
    CharacterSession *addSession(quint8 characterId,
                                 const QString &characterName,
                                 int characterDbId = 0,
                                 const QString &mudHost = QString(),
                                 quint16 mudPort = 0,
                                 const QString &loginUsername = QString(),
                                 const QString &loginPassword = QString(),
                                 bool autoReconnect = true,
                                 quint8 serverProfileId = 0,
                                 quint8 instanceId = 0);

    /**
     * @brief Returns the session for @p characterId, or @c nullptr if not found.
     * @param characterId The character identifier to look up.
     */
    CharacterSession *session(quint8 characterId) const;

    /**
     * @brief Returns all registered sessions in unspecified order.
     */
    QList<CharacterSession *> allSessions() const;

    /**
     * @brief Returns the number of registered sessions.
     */
    int sessionCount() const { return m_sessions.size(); }

    /**
     * @brief Requests a clean shutdown of all sessions.
     *
     * Calls stop() on each session and tracks how many are still stopping.
     * Emits allStopped() once every session has emitted its stopped() signal.
     * If there are no sessions, emits allStopped() immediately.
     */
    void stopAll();

signals:
    /**
     * @brief Forwarded from CharacterSession::runReady().
     *
     * DaemonServer connects to this signal to broadcast runs to all GUIs.
     *
     * @param characterId Source character.
     * @param run         The styled run to broadcast.
     */
    void runReady(quint8 characterId, StyledRun run);

    /**
     * @brief Forwarded from CharacterSession::statusChanged().
     * @param characterId Character whose status changed.
     * @param status      New status value.
     */
    void statusChanged(quint8 characterId, CharacterStatus status);

    /**
     * @brief Forwarded from CharacterSession::roomIdentified().
     * @param characterId Owning character.
     * @param roomId      Database room ID.
     * @param roomName    Room name string.
     */
    void roomIdentified(quint8 characterId, quint16 roomId,
                        const QString &roomName);

    /**
     * @brief Emitted when all sessions have stopped cleanly.
     *
     * Suitable for connecting to QCoreApplication::quit() for a clean
     * daemon shutdown sequence.
     */
    void allStopped();

private slots:
    /**
     * @brief Handles a session reporting that it has stopped.
     *
     * Calls QThread::quit() on the session's thread and decrements
     * the pending-stop counter. Emits allStopped() when the counter
     * reaches zero.
     *
     * @param characterId The character whose session stopped.
     */
    void onSessionStopped(quint8 characterId);

private:
    /**
     * @brief Internal storage for a session and its thread.
     */
    struct SessionEntry {
        CharacterSession *session = nullptr; ///< The session object.
        QThread          *thread  = nullptr; ///< The session's dedicated thread.
    };

    /**
     * @brief Starts the thread for @p entry, triggering CharacterSession::init().
     * @param entry The session entry to start.
     */
    void startSession(SessionEntry &entry);

    /**
     * @brief Forces @p entry's thread to quit, waiting up to 3 seconds.
     * @param entry The session entry to tear down.
     */
    void teardownSession(SessionEntry &entry);

    QMap<quint8, SessionEntry> m_sessions;    ///< All registered sessions by character ID.
    int                        m_pendingStops = 0; ///< Sessions still waiting to stop.
};
