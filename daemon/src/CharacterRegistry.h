#pragma once

#include <QObject>
#include <QMap>
#include <QThread>
#include "CharacterSession.h"

// ---------------------------------------------------------------------------
// CharacterRegistry
//
// Lives on the main thread. Owns all CharacterSession objects and their
// associated QThreads.
//
// Responsibilities:
//   - Create and start CharacterSessions
//   - Route input from GuiConnections to the correct session
//   - Forward session signals (runReady, statusChanged) to DaemonServer
//     for broadcast to all GUIs
//   - Clean shutdown of all sessions
//
// Threading:
//   - CharacterRegistry lives and operates entirely on the main thread
//   - Each CharacterSession runs on its own QThread
//   - Cross-thread communication is via queued signals/slots only
// ---------------------------------------------------------------------------

class CharacterRegistry : public QObject
{
    Q_OBJECT

public:
    explicit CharacterRegistry(QObject *parent = nullptr);
    ~CharacterRegistry() override;

    // Register a new character session. Creates the session and its thread,
    // moves the session to the thread, and starts it.
    // Returns the session pointer (owned by registry).
    CharacterSession *addSession(quint8 characterId,
                                 const QString &characterName);

    CharacterSession *session(quint8 characterId) const;
    QList<CharacterSession *> allSessions() const;
    int sessionCount() const { return m_sessions.size(); }

    // Request clean shutdown of all sessions. Emits allStopped() when done.
    void stopAll();

signals:
    // Forwarded from CharacterSession — broadcast to all GUIs by DaemonServer
    void runReady(quint8 characterId, StyledRun run);
    void statusChanged(quint8 characterId, CharacterStatus status);
    void roomIdentified(quint8 characterId, quint16 roomId,
                        const QString &roomName);

    // Emitted when all sessions have stopped cleanly.
    void allStopped();

private slots:
    void onSessionStopped(quint8 characterId);

private:
    struct SessionEntry {
        CharacterSession *session = nullptr;
        QThread          *thread  = nullptr;
    };

    void startSession(SessionEntry &entry);
    void teardownSession(SessionEntry &entry);

    QMap<quint8, SessionEntry> m_sessions;
    int m_pendingStops = 0;
};
