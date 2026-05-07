#include "GuiConnection.h"

#include <QDebug>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

GuiConnection::GuiConnection(QTcpSocket *socket, int connectionId,
                             QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_connectionId(connectionId)
{
    m_socket->setParent(this);

    connect(m_socket, &QTcpSocket::readyRead,
            this, &GuiConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &GuiConnection::onSocketDisconnected);
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &GuiConnection::onSocketError);

    m_pingTimer.setInterval(PING_INTERVAL_MS);
    m_pingTimer.setSingleShot(false);
    connect(&m_pingTimer, &QTimer::timeout,
            this, &GuiConnection::onPingTimer);
    m_pingTimer.start();
}

GuiConnection::~GuiConnection() = default;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QString GuiConnection::peerAddress() const
{
    if (m_socket == nullptr)
    {
        return QStringLiteral("(unknown)");
    }
    return QStringLiteral("%1:%2")
        .arg(m_socket->peerAddress().toString())
        .arg(m_socket->peerPort());
}

void GuiConnection::setController(bool controller)
{
    m_isController = controller;
}

void GuiConnection::sendFrame(quint8 msgType, quint8 characterId,
                              const QByteArray &payload)
{
    if (m_socket == nullptr || m_socket->state() != QAbstractSocket::ConnectedState)
    {
        return;
    }

    const quint16 payloadLen = static_cast<quint16>(payload.size());
    QByteArray frame = buildFrameHeader(msgType, characterId, payloadLen);
    frame.append(payload);
    m_socket->write(frame);
    m_txSequence++;
}

void GuiConnection::sendStyledRun(quint8 characterId, const StyledRun &run)
{
    // Wire format: [fg: 1][bg: 1][bold: 1][text_len: 2][text: N (UTF-8)]
    const QByteArray utf8 = run.text.toUtf8();
    const quint16 textLen = static_cast<quint16>(utf8.size());

    QByteArray payload;
    payload.reserve(5 + textLen);
    payload.append(static_cast<char>(run.fg));
    payload.append(static_cast<char>(run.bg));
    if (run.bold)
    {
        payload.append(static_cast<char>(1));
    }
    else
    {
        payload.append(static_cast<char>(0));
    }
    payload.append(static_cast<char>(textLen & 0xFF));
    payload.append(static_cast<char>((textLen >> 8) & 0xFF));
    payload.append(utf8);

    sendFrame(MSG_STYLED_RUN, characterId, payload);
}

void GuiConnection::sendControlStatus()
{
    QByteArray payload;
    if (m_isController)
    {
        payload.append(static_cast<char>(1));
    }
    else
    {
        payload.append(static_cast<char>(0));
    }
    sendFrame(MSG_CONTROL_STATUS, CHARACTER_ID_DAEMON, payload);
}

void GuiConnection::sendControlGranted()
{
    m_isController = true;
    sendFrame(MSG_CONTROL_GRANTED, CHARACTER_ID_DAEMON, QByteArray());
}

void GuiConnection::sendControlRevoked()
{
    m_isController = false;
    sendFrame(MSG_CONTROL_REVOKED, CHARACTER_ID_DAEMON, QByteArray());
}

void GuiConnection::markResyncComplete()
{
    m_resyncComplete = true;
}

void GuiConnection::disconnect(GoodbyeReason reason)
{
    m_pingTimer.stop();

    QByteArray payload;
    payload.append(static_cast<char>(reason));
    sendFrame(MSG_GOODBYE, CHARACTER_ID_DAEMON, payload);

    m_socket->disconnectFromHost();
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void GuiConnection::onReadyRead()
{
    m_rxBuffer.append(m_socket->readAll());
    processIncomingData();
}

void GuiConnection::onSocketDisconnected()
{
    m_pingTimer.stop();
    qDebug() << "GuiConnection" << m_connectionId << "disconnected";
    emit disconnected(this);
}

void GuiConnection::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    qWarning() << "GuiConnection" << m_connectionId
               << "socket error:" << m_socket->errorString();
}

void GuiConnection::onPingTimer()
{
    if (m_awaitingPong)
    {
        // No pong received in time — disconnect
        qWarning() << "GuiConnection" << m_connectionId
                   << "ping timeout — disconnecting";
        disconnect(GoodbyeReason::ProtocolError);
        return;
    }

    sendPing();
    m_awaitingPong = true;
}

// ---------------------------------------------------------------------------
// Frame reassembly
// ---------------------------------------------------------------------------

void GuiConnection::processIncomingData()
{
    while (m_rxBuffer.size() >= FRAME_HEADER_SIZE)
    {
        const quint8  version     = static_cast<quint8>(m_rxBuffer[0]);
        const quint8  msgType     = static_cast<quint8>(m_rxBuffer[1]);
        const quint8  characterId = static_cast<quint8>(m_rxBuffer[2]);
        // byte 3 = flags, reserved
        const quint16 sequence    = static_cast<quint8>(m_rxBuffer[4])
                                  | (static_cast<quint8>(m_rxBuffer[5]) << 8u);
        const quint16 payloadLen  = static_cast<quint8>(m_rxBuffer[6])
                                  | (static_cast<quint8>(m_rxBuffer[7]) << 8u);

        if (version != PROTOCOL_VERSION)
        {
            sendProtocolError(ProtocolErrorCode::VersionMismatch,
                              QStringLiteral("Unsupported protocol version"));
            disconnect(GoodbyeReason::ProtocolError);
            return;
        }

        const int totalSize = FRAME_HEADER_SIZE + payloadLen;
        if (m_rxBuffer.size() < totalSize)
        {
            break;
        }

        const QByteArray payload = m_rxBuffer.mid(FRAME_HEADER_SIZE, payloadLen);
        m_rxBuffer.remove(0, totalSize);

        handleFrame(msgType, characterId, sequence, payload);
    }
}

// ---------------------------------------------------------------------------
// Frame dispatch
// ---------------------------------------------------------------------------

void GuiConnection::handleFrame(quint8 msgType, quint8 characterId,
                                quint16 sequence, const QByteArray &payload)
{
    Q_UNUSED(sequence)

    // ClientHello must be first
    if (!m_helloReceived && msgType != MSG_CLIENT_HELLO)
    {
        sendProtocolError(ProtocolErrorCode::MalformedPayload,
                          QStringLiteral("Expected ClientHello"));
        disconnect(GoodbyeReason::ProtocolError);
        return;
    }

    switch (msgType)
    {
        case MSG_CLIENT_HELLO:
            handleClientHello(payload);
            return;

        case MSG_RESYNC_REQUEST:
            handleResyncRequest();
            return;

        case MSG_PONG:
            handlePong();
            return;

        case MSG_GOODBYE:
            // GUI is disconnecting cleanly — nothing to do, socket close follows
            return;

        case MSG_PING:
            // GUI pinging us — send pong
            sendFrame(MSG_PONG, CHARACTER_ID_DAEMON, QByteArray());
            return;

        case MSG_INPUT_ECHO:
            handleInput(characterId, payload);
            return;

        case MSG_SET_ENGINE_MODE:
            handleSetEngineMode(characterId, payload);
            return;

        case MSG_CLEAR_ATTENTION:
            handleClearAttention(characterId, payload);
            return;

        case MSG_STATS_RESET:
            handleStatsReset(characterId, payload);
            return;

        default:
            sendProtocolError(ProtocolErrorCode::UnknownMessageType,
                              QStringLiteral("Unknown message type 0x%1")
                                  .arg(msgType, 2, 16, QLatin1Char('0')));
            return;
    }
}

// ---------------------------------------------------------------------------
// Individual frame handlers
// ---------------------------------------------------------------------------

void GuiConnection::handleClientHello(const QByteArray &payload)
{
    if (m_helloReceived)
    {
        sendProtocolError(ProtocolErrorCode::MalformedPayload,
                          QStringLiteral("Duplicate ClientHello"));
        return;
    }

    if (payload.size() < 3)
    {
        sendProtocolError(ProtocolErrorCode::MalformedPayload,
                          QStringLiteral("ClientHello too short"));
        return;
    }

    const quint8 clientVersion = static_cast<quint8>(payload[0]);
    if (clientVersion != PROTOCOL_VERSION)
    {
        sendServerReject(static_cast<quint8>(ProtocolErrorCode::VersionMismatch),
                         QStringLiteral("Protocol version mismatch"));
        disconnect(GoodbyeReason::ProtocolError);
        return;
    }

    m_helloReceived = true;
    sendServerHello();

    qDebug() << "GuiConnection" << m_connectionId
             << "ClientHello accepted from" << peerAddress();
}

void GuiConnection::handleResyncRequest()
{
    if (!m_helloReceived)
    {
        sendProtocolError(ProtocolErrorCode::MalformedPayload,
                          QStringLiteral("ResyncRequest before ClientHello"));
        return;
    }

    qDebug() << "GuiConnection" << m_connectionId << "ResyncRequest received";
    emit resyncRequested(this);
}

void GuiConnection::handlePong()
{
    m_awaitingPong = false;
}

void GuiConnection::handleInput(quint8 characterId, const QByteArray &payload)
{
    // Reject input from observers and before resync is complete
    if (!m_isController)
    {
        // Silently discard — observer sending input is a GUI bug
        return;
    }

    if (!m_resyncComplete)
    {
        // Silently discard — input before resync is a GUI bug
        return;
    }

    // Wire format: [source: 1][text_len: 2][text: N (UTF-8)]
    if (payload.size() < 3)
    {
        return;
    }

    const quint16 textLen = static_cast<quint8>(payload[1])
                          | (static_cast<quint8>(payload[2]) << 8u);

    if (payload.size() < 3 + textLen)
    {
        return;
    }

    const QString text = QString::fromUtf8(payload.mid(3, textLen));
    emit inputReceived(characterId, text);
}

void GuiConnection::handleSetEngineMode(quint8 characterId,
                                        const QByteArray &payload)
{
    if (!m_isController || !m_resyncComplete || payload.size() < 2)
    {
        return;
    }

    // TODO: forward to CharacterRegistry → CharacterSession → PathEngine
    Q_UNUSED(characterId)
}

void GuiConnection::handleClearAttention(quint8 characterId,
                                         const QByteArray &payload)
{
    if (!m_isController || !m_resyncComplete || payload.size() < 3)
    {
        return;
    }

    // TODO: forward to CharacterRegistry → CharacterSession
    Q_UNUSED(characterId)
}

void GuiConnection::handleStatsReset(quint8 characterId,
                                     const QByteArray &payload)
{
    Q_UNUSED(payload)
    if (!m_isController || !m_resyncComplete)
    {
        return;
    }

    // TODO: forward to CharacterRegistry → CharacterSession → PathEngine
    Q_UNUSED(characterId)
}

// ---------------------------------------------------------------------------
// Outbound helpers
// ---------------------------------------------------------------------------

void GuiConnection::sendProtocolError(ProtocolErrorCode code,
                                      const QString &description)
{
    const QByteArray utf8 = description.toUtf8();
    const quint16 textLen = static_cast<quint16>(utf8.size());

    QByteArray payload;
    payload.append(static_cast<char>(code));
    payload.append(static_cast<char>(textLen & 0xFF));
    payload.append(static_cast<char>((textLen >> 8) & 0xFF));
    payload.append(utf8);

    sendFrame(MSG_PROTOCOL_ERROR, CHARACTER_ID_DAEMON, payload);
}

void GuiConnection::sendPing()
{
    sendFrame(MSG_PING, CHARACTER_ID_DAEMON, QByteArray());
}

void GuiConnection::sendServerHello()
{
    QByteArray payload;
    payload.append(static_cast<char>(PROTOCOL_VERSION));
    payload.append(static_cast<char>(0x00)); // server_flags — no auth required yet
    payload.append(static_cast<char>(0));    // num_characters (0 at hello time)
    sendFrame(MSG_SERVER_HELLO, CHARACTER_ID_DAEMON, payload);
}

void GuiConnection::sendServerReject(quint8 reasonCode, const QString &reason)
{
    const QByteArray utf8 = reason.toUtf8();
    QByteArray payload;
    payload.append(static_cast<char>(reasonCode));
    payload.append(static_cast<char>(utf8.size()));
    payload.append(utf8);
    sendFrame(MSG_SERVER_REJECT, CHARACTER_ID_DAEMON, payload);
}

QByteArray GuiConnection::buildFrameHeader(quint8 msgType, quint8 characterId,
                                           quint16 payloadLen) const
{
    QByteArray header(FRAME_HEADER_SIZE, '\0');
    header[0] = static_cast<char>(PROTOCOL_VERSION);
    header[1] = static_cast<char>(msgType);
    header[2] = static_cast<char>(characterId);
    header[3] = static_cast<char>(0x00); // flags — reserved
    header[4] = static_cast<char>(m_txSequence & 0xFF);
    header[5] = static_cast<char>((m_txSequence >> 8) & 0xFF);
    header[6] = static_cast<char>(payloadLen & 0xFF);
    header[7] = static_cast<char>((payloadLen >> 8) & 0xFF);
    return header;
}
