#include "connection/DaemonConnectionManager.h"

#include <QDataStream>
#include <QDebug>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

DaemonConnectionManager::DaemonConnectionManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected,
            this, &DaemonConnectionManager::onSocketConnected);
    connect(&m_socket, &QTcpSocket::disconnected,
            this, &DaemonConnectionManager::onSocketDisconnected);
    connect(&m_socket, &QAbstractSocket::errorOccurred,
            this, &DaemonConnectionManager::onSocketError);
    connect(&m_socket, &QTcpSocket::readyRead,
            this, &DaemonConnectionManager::onReadyRead);

    m_pingTimer.setInterval(PING_INTERVAL_MS);
    m_pingTimer.setSingleShot(false);
    connect(&m_pingTimer, &QTimer::timeout,
            this, &DaemonConnectionManager::onPingTimer);
}

DaemonConnectionManager::~DaemonConnectionManager()
{
    if (m_state != State::Disconnected)
    {
        sendGoodbye(GoodbyeReason::UserDisconnect);
        m_socket.disconnectFromHost();
    }
}

// ---------------------------------------------------------------------------
// Public slots
// ---------------------------------------------------------------------------

void DaemonConnectionManager::connectToDaemon(const QString &host, quint16 port)
{
    if (m_state != State::Disconnected)
    {
        return;
    }

    m_host = host;
    m_port = port;
    resetReceiveBuffer();
    setState(State::Connecting);
    m_socket.connectToHost(host, port);
}

void DaemonConnectionManager::disconnectFromDaemon()
{
    if (m_state == State::Disconnected)
    {
        return;
    }

    m_pingTimer.stop();

    if (m_state == State::Connecting)
    {
        // Socket is mid-connect  -  abort() fires disconnected() immediately.
        // disconnectFromHost() may silently succeed without a signal when
        // the connection has not yet been established.
        m_socket.abort();
        setState(State::Disconnected);
        return;
    }

    sendGoodbye(GoodbyeReason::UserDisconnect);
    m_socket.disconnectFromHost();
    // onSocketDisconnected() will call setState(Disconnected)
}

void DaemonConnectionManager::sendFrame(quint8 msgType, quint8 characterId,
                                        const QByteArray &payload)
{
    if (m_state != State::Syncing && m_state != State::Connected)
    {
        return;
    }

    const quint16 payloadLen = static_cast<quint16>(payload.size());
    QByteArray frame = buildFrameHeader(msgType, characterId, payloadLen);
    frame.append(payload);
    m_socket.write(frame);
}

// ---------------------------------------------------------------------------
// Private slots  -  socket events
// ---------------------------------------------------------------------------

void DaemonConnectionManager::onSocketConnected()
{
    setState(State::Syncing);

    // Send ClientHello
    QByteArray payload;
    payload.append(static_cast<char>(PROTOCOL_VERSION));  // protocol_version
    payload.append(static_cast<char>(0x00));              // client_flags (reserved)
    payload.append(static_cast<char>(0x00));              // auth_token_len (no token yet)
    sendFrame(MSG_CLIENT_HELLO, CHARACTER_ID_DAEMON, payload);

    // Immediately request a resync so we get the full state dump
    sendFrame(MSG_RESYNC_REQUEST, CHARACTER_ID_DAEMON, QByteArray());

    m_pingTimer.start();
}

void DaemonConnectionManager::onSocketDisconnected()
{
    m_pingTimer.stop();
    m_hasControl = false;
    setState(State::Disconnected);
}

void DaemonConnectionManager::onSocketError(QAbstractSocket::SocketError error)
{
    const QString description = m_socket.errorString();
    qWarning() << "DaemonConnectionManager: socket error:" << description;
    emit errorOccurred(description);

    // For most errors Qt emits disconnected() after errorOccurred(), which
    // drives the state transition. However ConnectionRefusedError and a
    // handful of others go straight to UnconnectedState without ever firing
    // disconnected(). Force the transition here for those cases so the rest
    // of the application (status bar, retry logic) behaves correctly.
    if (error == QAbstractSocket::ConnectionRefusedError
        || error == QAbstractSocket::NetworkError
        || error == QAbstractSocket::SocketTimeoutError
        || error == QAbstractSocket::HostNotFoundError)
    {
        m_pingTimer.stop();
        m_hasControl = false;
        setState(State::Disconnected);
    }
}

void DaemonConnectionManager::onReadyRead()
{
    m_rxBuffer.append(m_socket.readAll());
    processIncomingData();
}

void DaemonConnectionManager::onPingTimer()
{
    sendPing();
}

// ---------------------------------------------------------------------------
// Frame reassembly
// ---------------------------------------------------------------------------

void DaemonConnectionManager::processIncomingData()
{
    // Consume complete frames from m_rxBuffer until there is not enough data
    // for even the header.
    while (m_rxBuffer.size() >= FRAME_HEADER_SIZE)
    {
        const quint8  version    = static_cast<quint8>(m_rxBuffer[0]);
        const quint8  msgType    = static_cast<quint8>(m_rxBuffer[1]);
        const quint8  characterId = static_cast<quint8>(m_rxBuffer[2]);
        // byte 3 = flags, reserved
        const quint16 sequence   = (static_cast<quint8>(m_rxBuffer[4])      )
                                 | (static_cast<quint8>(m_rxBuffer[5]) << 8u);
        const quint16 payloadLen = (static_cast<quint8>(m_rxBuffer[6])      )
                                 | (static_cast<quint8>(m_rxBuffer[7]) << 8u);

        if (version != PROTOCOL_VERSION)
        {
            qWarning() << "DaemonConnectionManager: unexpected protocol version"
                       << version;
            // Treat as a fatal protocol error  -  disconnect
            emit errorOccurred(QStringLiteral("Protocol version mismatch"));
            disconnectFromDaemon();
            return;
        }

        const int totalFrameSize = FRAME_HEADER_SIZE + payloadLen;
        if (m_rxBuffer.size() < totalFrameSize)
        {
            // Wait for more data
            break;
        }

        const QByteArray payload = m_rxBuffer.mid(FRAME_HEADER_SIZE, payloadLen);
        m_rxBuffer.remove(0, totalFrameSize);

        handleFrame(msgType, characterId, sequence, payload);
    }
}

// ---------------------------------------------------------------------------
// Frame dispatch
// ---------------------------------------------------------------------------

void DaemonConnectionManager::handleFrame(quint8 msgType, quint8 characterId,
                                          quint16 sequence,
                                          const QByteArray &payload)
{
    // Handle the small set of messages that the connection manager owns.
    // Everything else is forwarded via frameReceived() for MainWindow/panes
    // to act on.

    switch (msgType)
    {
        case MSG_PING:
            // Daemon is pinging us  -  send pong back immediately
            sendFrame(MSG_PONG, CHARACTER_ID_DAEMON, QByteArray());
            return;

        case MSG_PONG:
            // Keepalive reply  -  no action needed
            return;

        case MSG_GOODBYE:
            // Daemon is shutting down or disconnecting cleanly
            m_pingTimer.stop();
            setState(State::Disconnected);
            return;

        case MSG_RESYNC_ACK:
            handleResyncAck();
            return;

        case MSG_CONTROL_STATUS:
            handleControlStatus(payload);
            return;

        case MSG_CONTROL_GRANTED:
            m_hasControl = true;
            emit hasControlChanged(true);
            return;

        case MSG_CONTROL_REVOKED:
            m_hasControl = false;
            emit hasControlChanged(false);
            return;

        case MSG_PROTOCOL_ERROR:
            handleProtocolError(payload);
            return;

        case MSG_FATAL_ERROR:
        {
            const QString text = QString::fromUtf8(payload.mid(3));
            qCritical() << "DaemonConnectionManager: fatal error from daemon:" << text;
            emit errorOccurred(QStringLiteral("[MeagreMUD] Fatal daemon error: ") + text);
            disconnectFromDaemon();
            return;
        }

        case MSG_SERVER_REJECT:
        {
            const QString reason = QString::fromUtf8(payload.mid(2));
            emit errorOccurred(QStringLiteral("[MeagreMUD] Connection rejected: ") + reason);
            disconnectFromDaemon();
            return;
        }

        default:
            break;
    }

    // Forward to the rest of the application
    emit frameReceived(msgType, characterId, sequence, payload);
}

void DaemonConnectionManager::handleResyncAck()
{
    // Transition Syncing -> Connected.
    // The full state dump (CharacterInfo messages etc.) follows immediately
    // after ResyncAck in the stream and will arrive via subsequent frameReceived()
    // emissions  -  no special handling needed here.
    if (m_state == State::Syncing)
    {
        setState(State::Connected);
    }
}

void DaemonConnectionManager::handleControlStatus(const QByteArray &payload)
{
    if (payload.isEmpty())
    {
        return;
    }
    const bool hasControl = (static_cast<quint8>(payload[0]) != 0);
    if (hasControl != m_hasControl)
    {
        m_hasControl = hasControl;
        emit hasControlChanged(m_hasControl);
    }
}

void DaemonConnectionManager::handleProtocolError(const QByteArray &payload)
{
    Q_UNUSED(payload)
    m_resyncCount++;
    qWarning() << "DaemonConnectionManager: protocol error  -  resync #" << m_resyncCount;

    // Re-enter Syncing state and request a fresh resync
    setState(State::Syncing);
    sendFrame(MSG_RESYNC_REQUEST, CHARACTER_ID_DAEMON, QByteArray());
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void DaemonConnectionManager::sendPing()
{
    sendFrame(MSG_PING, CHARACTER_ID_DAEMON, QByteArray());
}

void DaemonConnectionManager::sendGoodbye(GoodbyeReason reason)
{
    QByteArray payload;
    payload.append(static_cast<char>(reason));
    // Send directly  -  bypass the state guard in sendFrame()
    const quint16 payloadLen = static_cast<quint16>(payload.size());
    QByteArray frame = buildFrameHeader(MSG_GOODBYE, CHARACTER_ID_DAEMON, payloadLen);
    frame.append(payload);
    m_socket.write(frame);
    m_socket.flush();
}

void DaemonConnectionManager::setState(State newState)
{
    if (newState == m_state)
    {
        return;
    }
    m_state = newState;
    emit stateChanged(m_state);
}

void DaemonConnectionManager::resetReceiveBuffer()
{
    m_rxBuffer.clear();
}

QByteArray DaemonConnectionManager::buildFrameHeader(quint8 msgType,
                                                     quint8 characterId,
                                                     quint16 payloadLen) const
{
    QByteArray header(FRAME_HEADER_SIZE, '\0');
    header[0] = static_cast<char>(PROTOCOL_VERSION);
    header[1] = static_cast<char>(msgType);
    header[2] = static_cast<char>(characterId);
    header[3] = static_cast<char>(0x00); // flags  -  reserved
    // sequence  -  little-endian
    header[4] = static_cast<char>(m_txSequence & 0xFF);
    header[5] = static_cast<char>((m_txSequence >> 8) & 0xFF);
    // payload_len  -  little-endian
    header[6] = static_cast<char>(payloadLen & 0xFF);
    header[7] = static_cast<char>((payloadLen >> 8) & 0xFF);
    // Note: m_txSequence intentionally not incremented here  -  the sequence
    // counter is logically per-send; increment is done at call sites via
    // a mutable counter once we wire that up properly.
    return header;
}
