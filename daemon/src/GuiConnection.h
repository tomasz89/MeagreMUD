#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QTimer>
#include "protocol/Protocol.h"
#include "types/MudTypes.h"

/**
 * @file GuiConnection.h
 * @brief Represents one connected GUI client on the daemon side.
 */

/**
 * @brief Manages the protocol connection to a single GUI client.
 *
 * Lives on the main thread alongside DaemonServer. One instance is created
 * per accepted TCP connection.
 *
 * @par Responsibilities
 * - Frame reassembly from the incoming byte stream.
 * - Dispatching inbound frames to appropriate handlers.
 * - Sending outbound frames to the GUI.
 * - Ping/pong keepalive with disconnect-on-timeout.
 * - Enforcing controller/observer roles and resync completion gating.
 *
 * @par Controller arbitration
 * - The first GUI to complete resync becomes the controller.
 * - On controller disconnect, DaemonServer promotes the oldest remaining GUI.
 * - Observer GUIs receive all output but their input frames are silently discarded.
 *
 * @par Resync gating
 * Input frames and automation control frames are silently discarded until
 * @c markResyncComplete() has been called by DaemonServer.
 */
class GuiConnection : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the connection, taking ownership of @p socket.
     * @param socket       The accepted TCP socket. Ownership is transferred.
     * @param connectionId Unique monotonic identifier assigned by DaemonServer.
     * @param parent       Optional Qt parent object.
     */
    explicit GuiConnection(QTcpSocket *socket, int connectionId,
                           QObject *parent = nullptr);

    /// @brief Destructor.
    ~GuiConnection() override;

    /// @brief Returns the unique connection identifier.
    int connectionId() const { return m_connectionId; }

    /// @brief Returns @c true if this GUI is the current controller.
    bool isController() const { return m_isController; }

    /// @brief Returns @c true if the resync dump has been completed.
    bool resyncComplete() const { return m_resyncComplete; }

    /**
     * @brief Returns a human-readable peer address string ("host:port").
     */
    QString peerAddress() const;

    /**
     * @brief Sets or clears the controller role without sending a protocol message.
     *
     * Use sendControlGranted() / sendControlRevoked() to both update the flag
     * and notify the GUI.
     *
     * @param controller @c true to mark as controller.
     */
    void setController(bool controller);

    /**
     * @brief Sends a fully assembled frame to this GUI.
     *
     * No-op if the socket is not in ConnectedState.
     *
     * @param msgType     Message type byte.
     * @param characterId Character this frame relates to, or CHARACTER_ID_DAEMON.
     * @param payload     Frame payload (without the 8-byte header).
     */
    void sendFrame(quint8 msgType, quint8 characterId,
                   const QByteArray &payload);

    /**
     * @brief Encodes and sends a StyledRun for a character.
     *
     * Wire format: @c [fg:1][bg:1][bold:1][text_len:2][text:N]
     *
     * @param characterId The character this run belongs to.
     * @param run         The styled run to send.
     */
    void sendStyledRun(quint8 characterId, const StyledRun &run);

    /**
     * @brief Sends MSG_CONTROL_STATUS reflecting current @c m_isController state.
     */
    void sendControlStatus();

    /**
     * @brief Sets @c m_isController = true and sends MSG_CONTROL_GRANTED.
     */
    void sendControlGranted();

    /**
     * @brief Sets @c m_isController = false and sends MSG_CONTROL_REVOKED.
     */
    void sendControlRevoked();

    /**
     * @brief Marks the resync dump as complete, allowing input frames.
     *
     * Called by DaemonServer after sending the full state dump following
     * MSG_RESYNC_ACK.
     */
    void markResyncComplete();

    /**
     * @brief Sends MSG_GOODBYE and closes the socket.
     * @param reason The reason code to include in the frame.
     */
    void disconnect(GoodbyeReason reason = GoodbyeReason::ShuttingDown);

signals:
    /**
     * @brief Emitted when a valid input frame is received from the controller.
     *
     * Only emitted if @c m_isController and @c m_resyncComplete are both true.
     *
     * @param characterId Target character.
     * @param text        Input text (already @c \\r\\n terminated).
     */
    void inputReceived(quint8 characterId, const QString &text);

    /**
     * @brief Emitted when the GUI sends MSG_RESYNC_REQUEST.
     *
     * DaemonServer responds by sending ResyncAck then the full state dump.
     *
     * @param connection Pointer to this connection (for DaemonServer routing).
     */
    void resyncRequested(GuiConnection *connection);

    /**
     * @brief Emitted when the TCP socket disconnects for any reason.
     * @param connection Pointer to this connection.
     */
    void disconnected(GuiConnection *connection);

    /**
     * @brief Emitted when an unrecoverable protocol error is detected.
     *
     * The connection sends MSG_PROTOCOL_ERROR to the GUI before emitting
     * this signal. DaemonServer should expect the socket to close shortly.
     *
     * @param connection  Pointer to this connection.
     * @param code        Protocol error code.
     * @param description Human-readable error description.
     */
    void protocolError(GuiConnection *connection,
                       ProtocolErrorCode code, const QString &description);

private slots:
    /// @brief Appends available socket data to @c m_rxBuffer and processes frames.
    void onReadyRead();

    /// @brief Stops the ping timer and emits disconnected().
    void onSocketDisconnected();

    /**
     * @brief Logs the socket error. The socket will close and onSocketDisconnected() follows.
     * @param error The socket error code.
     */
    void onSocketError(QAbstractSocket::SocketError error);

    /**
     * @brief Sends a ping or disconnects if the previous ping went unanswered.
     *
     * Called by @c m_pingTimer on each interval. If @c m_awaitingPong is
     * already set, the GUI has not responded in time and is disconnected.
     */
    void onPingTimer();

private:
    /**
     * @brief Consumes complete frames from @c m_rxBuffer and dispatches them.
     *
     * Leaves any incomplete trailing frame in the buffer for the next call.
     */
    void processIncomingData();

    /**
     * @brief Dispatches a single complete inbound frame.
     * @param msgType     Message type byte.
     * @param characterId character_id from the frame header.
     * @param sequence    Sequence number from the frame header.
     * @param payload     Frame payload bytes.
     */
    void handleFrame(quint8 msgType, quint8 characterId,
                     quint16 sequence, const QByteArray &payload);

    /// @brief Handles MSG_CLIENT_HELLO. Validates version and sends ServerHello.
    void handleClientHello(const QByteArray &payload);

    /// @brief Handles MSG_RESYNC_REQUEST. Emits resyncRequested().
    void handleResyncRequest();

    /// @brief Handles MSG_PONG. Clears @c m_awaitingPong.
    void handlePong();

    /**
     * @brief Handles MSG_INPUT_ECHO. Emits inputReceived() if authorised.
     * @param characterId Target character.
     * @param payload     Raw frame payload.
     */
    void handleInput(quint8 characterId, const QByteArray &payload);

    /**
     * @brief Handles MSG_SET_ENGINE_MODE.
     * @param characterId Target character.
     * @param payload     Raw frame payload.
     */
    void handleSetEngineMode(quint8 characterId, const QByteArray &payload);

    /**
     * @brief Handles MSG_CLEAR_ATTENTION.
     * @param characterId Target character.
     * @param payload     Raw frame payload.
     */
    void handleClearAttention(quint8 characterId, const QByteArray &payload);

    /**
     * @brief Handles MSG_STATS_RESET.
     * @param characterId Target character.
     * @param payload     Raw frame payload (unused currently).
     */
    void handleStatsReset(quint8 characterId, const QByteArray &payload);

    /**
     * @brief Sends MSG_PROTOCOL_ERROR with @p code and @p description.
     * @param code        Error code.
     * @param description Human-readable description.
     */
    void sendProtocolError(ProtocolErrorCode code, const QString &description);

    /// @brief Sends MSG_PING to the GUI.
    void sendPing();

    /// @brief Sends MSG_SERVER_HELLO after a valid ClientHello.
    void sendServerHello();

    /**
     * @brief Sends MSG_SERVER_REJECT and prepares for disconnect.
     * @param reasonCode Numeric reason code.
     * @param reason     Human-readable reason string.
     */
    void sendServerReject(quint8 reasonCode, const QString &reason);

    /**
     * @brief Builds an 8-byte frame header for outbound frames.
     * @param msgType     Message type byte.
     * @param characterId character_id field value.
     * @param payloadLen  Length of the payload in bytes.
     * @return The 8-byte header as a QByteArray.
     */
    QByteArray buildFrameHeader(quint8 msgType, quint8 characterId,
                                quint16 payloadLen) const;

    QTcpSocket *m_socket;                        ///< Owned TCP socket.
    int         m_connectionId;                  ///< Unique connection identifier.
    bool        m_isController    = false;       ///< True if this GUI is the controller.
    bool        m_resyncComplete  = false;       ///< True after DaemonServer sends the full dump.
    bool        m_helloReceived   = false;       ///< True after a valid ClientHello.

    QByteArray  m_rxBuffer;                      ///< Inbound frame reassembly buffer.
    quint16     m_txSequence = 0;                ///< Outbound frame sequence counter.

    QTimer      m_pingTimer;                     ///< Periodic keepalive timer.
    static constexpr int PING_INTERVAL_MS = 30000; ///< Ping interval in milliseconds.
    static constexpr int PING_TIMEOUT_MS  = 10000; ///< Pong timeout in milliseconds.
    bool        m_awaitingPong = false;           ///< True if a ping is outstanding.
};
