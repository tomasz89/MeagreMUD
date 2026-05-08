#pragma once

/**
 * @file DaemonConnectionManager.h
 * @brief GUI-side TCP connection to the MeagreMUD daemon.
 */

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include "protocol/Protocol.h"

/**
 * @brief Manages the GUI's TCP connection to the daemon and drives the
 *        GUI connection state machine.
 *
 * The state machine:
 * @code
 * Disconnected → Connecting → Syncing → Connected
 *                                ↑
 *                     (re-enters on ProtocolError)
 * @endcode
 *
 * All state transitions are signalled via stateChanged() so the rest of the
 * GUI can react without polling or knowing about the socket directly. All
 * inputs are locked by MainWindow until the Connected state is reached.
 *
 * Frame reassembly: the daemon sends a continuous binary stream. Incoming
 * bytes are buffered internally; complete frames are emitted via
 * frameReceived(). The 8-byte frame header is stripped before emission.
 */
class DaemonConnectionManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief GUI connection state.
     */
    enum class State {
        Disconnected, ///< No socket; not attempting to connect.
        Connecting,   ///< TCP connect in progress.
        Syncing,      ///< Connected; ClientHello sent; waiting for ResyncAck.
        Connected,    ///< ResyncAck received; normal operation.
    };
    Q_ENUM(State)

    /**
     * @brief Construct the manager.
     * @param parent Qt parent object.
     */
    explicit DaemonConnectionManager(QObject *parent = nullptr);

    /// Sends Goodbye and closes the socket if connected.
    ~DaemonConnectionManager() override;

    /// @return Current connection state.
    State   state()      const { return m_state; }

    /// @return @c true if this GUI is currently the daemon controller.
    bool    hasControl() const { return m_hasControl; }

    /// @return The host name or IP address last passed to connectToDaemon().
    QString host()       const { return m_host; }

    /// @return The port number last passed to connectToDaemon().
    quint16 port()       const { return m_port; }

    /**
     * @brief Number of protocol resyncs that have occurred this session.
     *
     * Increments each time Syncing is re-entered after a ProtocolError.
     * Displayed as "[MeagreMUD] Protocol resync #N".
     */
    int resyncCount() const { return m_resyncCount; }

public slots:
    /**
     * @brief Open a TCP connection to the daemon and begin the handshake.
     *
     * No-op if not in Disconnected state.
     *
     * @param host  Hostname or IP address.
     * @param port  TCP port.
     */
    void connectToDaemon(const QString &host, quint16 port);

    /**
     * @brief Send Goodbye and close the socket.
     *
     * No-op if already in Disconnected state. Safe to call from any state.
     */
    void disconnectFromDaemon();

    /**
     * @brief Send a fully assembled frame to the daemon.
     *
     * No-op if the state is not Syncing or Connected. The frame header is
     * built internally; callers supply only the message type, character ID,
     * and raw payload.
     *
     * @param msgType     Message type byte (see Protocol.h MSG_* constants).
     * @param characterId Target character (CHARACTER_ID_DAEMON for non-character frames).
     * @param payload     Raw payload bytes (without the 8-byte header).
     */
    void sendFrame(quint8 msgType, quint8 characterId, const QByteArray &payload);

signals:
    /**
     * @brief Emitted whenever the connection state changes.
     * @param newState The state just entered.
     */
    void stateChanged(DaemonConnectionManager::State newState);

    /**
     * @brief Emitted when the controller/observer status changes.
     * @param hasControl @c true if this GUI is now the controller.
     */
    void hasControlChanged(bool hasControl);

    /**
     * @brief Emitted for each complete, reassembled frame from the daemon.
     *
     * The 8-byte frame header is stripped; @p payload contains only the
     * message-specific bytes.
     *
     * @param msgType     Message type byte.
     * @param characterId Source character (or CHARACTER_ID_DAEMON).
     * @param sequence    Frame sequence number from the header.
     * @param payload     Raw payload bytes.
     */
    void frameReceived(quint8 msgType, quint8 characterId, quint16 sequence,
                       const QByteArray &payload);

    /**
     * @brief Emitted when a socket or protocol error occurs.
     *
     * Always accompanied by a stateChanged(Disconnected) if the connection
     * was lost as a result.
     *
     * @param description Human-readable error description.
     */
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

    QByteArray buildFrameHeader(quint8 msgType, quint8 characterId,
                                quint16 payloadLen) const;

    QTcpSocket  m_socket;
    State       m_state       = State::Disconnected;
    bool        m_hasControl  = false;
    QString     m_host;
    quint16     m_port        = 0;
    quint16     m_txSequence  = 0;
    int         m_resyncCount = 0;
    QByteArray  m_rxBuffer;
    QTimer      m_pingTimer;

    /// Keepalive interval in milliseconds.
    static constexpr int PING_INTERVAL_MS = 30000;
};
