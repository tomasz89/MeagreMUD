#pragma once

#include <QObject>
#include <QThread>
#include <QTcpSocket>
#include <QString>
#include "protocol/Protocol.h"
#include "types/MudTypes.h"

// ---------------------------------------------------------------------------
// CharacterSession
//
// Manages one character's connection to the MUD server. Runs on its own
// QThread — one thread per character.
//
// Threading rules:
//   - CharacterSession is created on the main thread but MUST NOT create
//     its QTcpSocket in the constructor. The socket is created in init(),
//     which is called via a queued connection after the thread starts,
//     ensuring the socket belongs to the session thread.
//   - All signals crossing the thread boundary use Qt::QueuedConnection
//     (automatic for cross-thread signals).
//   - CharacterRegistry holds ownership; CharacterSession must not be
//     deleted directly — stop() then deleteLater().
//
// Lifecycle:
//   main thread creates CharacterSession
//   main thread creates QThread, moves CharacterSession to it
//   QThread::started → CharacterSession::init() [queued]
//   init() creates QTcpSocket, connects to MUD
//   normal operation: MUD bytes → AnsiParser → StyledRun → broadcast
//   stop(): disconnect MUD socket, quit thread
// ---------------------------------------------------------------------------

class CharacterSession : public QObject
{
    Q_OBJECT

public:
    explicit CharacterSession(quint8 characterId,
                              const QString &characterName,
                              QObject *parent = nullptr);
    ~CharacterSession() override;

    quint8          characterId()   const { return m_characterId; }
    QString         characterName() const { return m_characterName; }
    CharacterStatus status()        const { return m_status; }

    // Called from the main thread to request a clean shutdown.
    // Emits stopped() when complete.
    void stop();

signals:
    // Emitted when a StyledRun is ready to broadcast to all GUIs.
    // Connected with Qt::QueuedConnection from DaemonServer.
    void runReady(quint8 characterId, StyledRun run);

    // Emitted when the character's connection status changes.
    void statusChanged(quint8 characterId, CharacterStatus status);

    // Emitted when a room has been identified from the output stream.
    void roomIdentified(quint8 characterId, quint16 roomId,
                        const QString &roomName);

    // Emitted once the session thread has fully stopped.
    void stopped(quint8 characterId);

public slots:
    // Called via queued connection from QThread::started.
    // Creates the MUD socket here — never in the constructor.
    void init();

    // Forward input text to the MUD socket.
    // Called from main thread via queued connection.
    void sendInput(const QString &text);

private slots:
    void onMudConnected();
    void onMudDisconnected();
    void onMudError(QAbstractSocket::SocketError error);
    void onMudReadyRead();

private:
    void setStatus(CharacterStatus status);
    void connectToMud();

    quint8          m_characterId;
    QString         m_characterName;
    CharacterStatus m_status = CharacterStatus::Disconnected;

    // MUD-side socket — created in init(), owned by this object's thread
    QTcpSocket     *m_mudSocket = nullptr;

    // MUD connection parameters (populated before thread starts)
    QString         m_mudHost;
    quint16         m_mudPort = 0;

    bool            m_stopRequested = false;
};
