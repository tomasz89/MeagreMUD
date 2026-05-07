#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include "protocol/Protocol.h"

// ---------------------------------------------------------------------------
// DaemonConnectionManager
//
// Owns the TCP connection to the daemon and drives the GUI connection state
// machine:
//
//   Disconnected → Connecting → Syncing → Connected
//                                  ↑
//                       (re-enters on ProtocolError)
//
// All transitions are signalled so the rest of the GUI can react without
// polling or knowing about the socket directly.
//
// Frame reassembly: the daemon sends a continuous binary stream. Incoming
// bytes are buffered here; complete frames are emitted via frameReceived().
// ---------------------------------------------------------------------------

class DaemonConnectionManager : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Syncing,      // TCP connected, waiting for ResyncAck
        Connected,    // ResyncAck received — normal operation
    };
    Q_ENUM(State)

    explicit DaemonConnectionManager(QObject *parent = nullptr);
    ~DaemonConnectionManager() override;

    State       state()    const { return m_state; }
    bool        hasControl() const { return m_hasControl; }
    QString     host()     const { return m_host; }
    quint16     port()     const { return m_port; }

    // Resync counter — increments each time we re-enter Syncing after
    // a ProtocolError. Displayed as "[MeagreMUD] Protocol resync #N".
    int resyncCount() const { return m_resyncCount; }

public slots:
    void connectToDaemon(const QString &host, quint16 port);
    void disconnectFromDaemon();

    // Send a fully-assembled frame (header + payload) to the daemon.
    // No-op if not in Syncing or Connected state.
    void sendFrame(quint8 msgType, quint8 characterId, const QByteArray &payload);

signals:
    void stateChanged(DaemonConnectionManager::State newState);
    void hasControlChanged(bool hasControl);

    // Emitted for each complete, reassembled frame received from the daemon.
    // Callers match on msgType and characterId; payload does not include the
    // 8-byte frame header.
    void frameReceived(quint8 msgType, quint8 characterId, quint16 sequence,
                       const QByteArray &payload);

    // Convenience signal — emitted alongside stateChanged when an error
    // caused the disconnect, with a human-readable description.
    void errorOccurred(const QString &description);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReadyRead();
    void onPingTimer();

private:
    void setState(State newState);
    void processIncomingData();
    void handleFrame(quint8 msgType, quint8 characterId, quint16 sequence,
                     const QByteArray &payload);
    void handleResyncAck();
    void handleControlStatus(const QByteArray &payload);
    void handleProtocolError(const QByteArray &payload);
    void sendPing();
    void sendGoodbye(GoodbyeReason reason);
    void resetReceiveBuffer();

    // Frame assembly helpers
    QByteArray buildFrameHeader(quint8 msgType, quint8 characterId,
                                quint16 payloadLen) const;

    QTcpSocket  m_socket;
    State       m_state       = State::Disconnected;
    bool        m_hasControl  = false;
    QString     m_host;
    quint16     m_port        = 0;
    quint16     m_txSequence  = 0;
    int         m_resyncCount = 0;

    // Incoming frame reassembly buffer
    QByteArray  m_rxBuffer;

    // Keepalive
    QTimer      m_pingTimer;
    static constexpr int PING_INTERVAL_MS = 30000;
};
