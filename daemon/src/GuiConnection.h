#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QTimer>
#include "protocol/Protocol.h"
#include "types/MudTypes.h"

// ---------------------------------------------------------------------------
// GuiConnection
//
// Represents one connected GUI client. Lives on the main thread alongside
// DaemonServer. One instance per connected GUI.
//
// Responsibilities:
//   - Frame reassembly from the incoming byte stream
//   - Frame dispatch to DaemonServer for handling
//   - Outbound frame queuing and sending
//   - Ping/pong keepalive
//   - Tracking whether this GUI is the controller
//
// Controller arbitration:
//   - First GUI to complete resync becomes controller
//   - On controller disconnect, DaemonServer promotes the oldest remaining GUI
//   - Observer GUIs receive all output but their input frames are rejected
//
// Resync:
//   - On ClientHello + ResyncRequest, DaemonServer sends the full state dump
//   - ResyncAck is sent before the dump begins
//   - Input frames are rejected until resync is complete (m_resyncComplete)
// ---------------------------------------------------------------------------

class GuiConnection : public QObject
{
    Q_OBJECT

public:
    explicit GuiConnection(QTcpSocket *socket, int connectionId,
                           QObject *parent = nullptr);
    ~GuiConnection() override;

    int    connectionId()    const { return m_connectionId; }
    bool   isController()    const { return m_isController; }
    bool   resyncComplete()  const { return m_resyncComplete; }
    QString peerAddress()    const;

    void setController(bool controller);

    // Send a fully assembled frame to this GUI.
    void sendFrame(quint8 msgType, quint8 characterId,
                   const QByteArray &payload);

    // Convenience: send a StyledRun for a character.
    void sendStyledRun(quint8 characterId, const StyledRun &run);

    // Send the control status message reflecting current controller state.
    void sendControlStatus();

    // Send ControlGranted / ControlRevoked.
    void sendControlGranted();
    void sendControlRevoked();

    // Begin the resync dump sequence for this GUI.
    // DaemonServer calls this after sending ResyncAck.
    void markResyncComplete();

    // Request disconnect — sends Goodbye then closes.
    void disconnect(GoodbyeReason reason = GoodbyeReason::ShuttingDown);

signals:
    // Emitted when a complete, valid input frame has been received and
    // the GUI is authorised to send input (controller + resync complete).
    void inputReceived(quint8 characterId, const QString &text);

    // Emitted when the GUI requests a resync.
    void resyncRequested(GuiConnection *connection);

    // Emitted when the socket disconnects for any reason.
    void disconnected(GuiConnection *connection);

    // Emitted when an unrecoverable protocol error is detected.
    void protocolError(GuiConnection *connection,
                       ProtocolErrorCode code, const QString &description);

private slots:
    void onReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onPingTimer();

private:
    void processIncomingData();
    void handleFrame(quint8 msgType, quint8 characterId,
                     quint16 sequence, const QByteArray &payload);

    void handleClientHello(const QByteArray &payload);
    void handleResyncRequest();
    void handlePong();
    void handleInput(quint8 characterId, const QByteArray &payload);
    void handleSetEngineMode(quint8 characterId, const QByteArray &payload);
    void handleClearAttention(quint8 characterId, const QByteArray &payload);
    void handleStatsReset(quint8 characterId, const QByteArray &payload);

    void sendProtocolError(ProtocolErrorCode code, const QString &description);
    void sendPing();
    void sendServerHello();
    void sendServerReject(quint8 reasonCode, const QString &reason);

    QByteArray buildFrameHeader(quint8 msgType, quint8 characterId,
                                quint16 payloadLen) const;

    QTcpSocket *m_socket;
    int         m_connectionId;
    bool        m_isController    = false;
    bool        m_resyncComplete  = false;
    bool        m_helloReceived   = false;

    // Frame reassembly
    QByteArray  m_rxBuffer;
    quint16     m_txSequence = 0;

    // Keepalive
    QTimer      m_pingTimer;
    static constexpr int PING_INTERVAL_MS  = 30000;
    static constexpr int PING_TIMEOUT_MS   = 10000;
    bool        m_awaitingPong = false;
};
