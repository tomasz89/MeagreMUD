#include "DaemonServer.h"

#include <QDebug>
#include <QHostAddress>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

DaemonServer::DaemonServer(CharacterRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
{
    connect(&m_server, &QTcpServer::newConnection,
            this, &DaemonServer::onNewConnection);

    // Wire registry signals to broadcast slots
    connect(m_registry, &CharacterRegistry::runReady,
            this, &DaemonServer::broadcastStyledRun);
    connect(m_registry, &CharacterRegistry::statusChanged,
            this, &DaemonServer::broadcastStatusChange);
    connect(m_registry, &CharacterRegistry::roomIdentified,
            this, &DaemonServer::broadcastRoomIdentified);
}

DaemonServer::~DaemonServer()
{
    stopListening();
    for (GuiConnection *conn : m_connections)
    {
        conn->disconnect(GoodbyeReason::ShuttingDown);
    }
    qDeleteAll(m_connections);
    m_connections.clear();
}

// ---------------------------------------------------------------------------
// Listen / stop
// ---------------------------------------------------------------------------

bool DaemonServer::listen(const QHostAddress &address, quint16 port)
{
    if (!m_server.listen(address, port))
    {
        qCritical() << "DaemonServer: failed to listen on"
                    << address.toString() << ":" << port
                    << " - " << m_server.errorString();
        return false;
    }

    qInfo() << "DaemonServer: listening on"
            << address.toString() << ":" << m_server.serverPort();
    return true;
}

void DaemonServer::stopListening()
{
    if (m_server.isListening())
    {
        m_server.close();
    }
}

bool DaemonServer::isListening() const
{
    return m_server.isListening();
}

quint16 DaemonServer::serverPort() const
{
    return m_server.serverPort();
}

// ---------------------------------------------------------------------------
// Broadcast slots  -  called from main thread via queued connection
// ---------------------------------------------------------------------------

void DaemonServer::broadcastStyledRun(quint8 characterId, StyledRun run)
{
    for (GuiConnection *conn : m_connections)
    {
        conn->sendStyledRun(characterId, run);
    }
}

void DaemonServer::broadcastStatusChange(quint8 characterId,
                                         CharacterStatus status)
{
    QByteArray payload;
    payload.append(static_cast<char>(status));

    for (GuiConnection *conn : m_connections)
    {
        conn->sendFrame(MSG_STATUS_CHANGE, characterId, payload);
    }
}

void DaemonServer::broadcastRoomIdentified(quint8 characterId, quint16 roomId,
                                           const QString &roomName)
{
    const QByteArray nameUtf8 = roomName.toUtf8();
    const quint8 nameLen = static_cast<quint8>(
        qMin(nameUtf8.size(), 255));

    QByteArray payload;
    payload.append(static_cast<char>(roomId & 0xFF));
    payload.append(static_cast<char>((roomId >> 8) & 0xFF));
    payload.append(static_cast<char>(nameLen));
    payload.append(nameUtf8.left(nameLen));

    for (GuiConnection *conn : m_connections)
    {
        conn->sendFrame(MSG_ROOM_IDENTIFIED, characterId, payload);
    }
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void DaemonServer::onNewConnection()
{
    while (m_server.hasPendingConnections())
    {
        QTcpSocket *socket = m_server.nextPendingConnection();
        if (socket == nullptr)
        {
            continue;
        }

        auto *conn = new GuiConnection(socket, m_nextConnectionId++, this);

        connect(conn, &GuiConnection::disconnected,
                this, &DaemonServer::onGuiDisconnected);
        connect(conn, &GuiConnection::resyncRequested,
                this, &DaemonServer::onGuiResyncRequested);
        connect(conn, &GuiConnection::inputReceived,
                this, &DaemonServer::onGuiInputReceived);
        connect(conn, &GuiConnection::protocolError,
                this, &DaemonServer::onGuiProtocolError);

        m_connections.append(conn);

        qInfo() << "DaemonServer: GUI connected from"
                << conn->peerAddress()
                << "(connection" << conn->connectionId() << ")";
    }
}

void DaemonServer::onGuiDisconnected(GuiConnection *connection)
{
    const bool wasController = connection->isController();
    QString connInfo = QStringLiteral("DaemonServer: GUI disconnected %1 (connection %2")
        .arg(connection->peerAddress())
        .arg(connection->connectionId());
    if (wasController)
    {
        connInfo += QStringLiteral("  -  was controller)");
    }
    else
    {
        connInfo += QStringLiteral(")");
    }
    qInfo() << connInfo;

    removeConnection(connection);

    if (wasController)
    {
        promoteNewController();
    }
}

void DaemonServer::onGuiResyncRequested(GuiConnection *connection)
{
    qDebug() << "DaemonServer: resync requested by connection"
             << connection->connectionId();

    // Send ResyncAck first
    connection->sendFrame(MSG_RESYNC_ACK, CHARACTER_ID_DAEMON, QByteArray());

    // Send full state dump
    sendResyncDump(connection);

    // Mark resync complete
    connection->markResyncComplete();

    // Promote to controller if no controller exists yet
    if (controllerConnection() == nullptr)
    {
        connection->setController(true);
        connection->sendControlGranted();
        qInfo() << "DaemonServer: connection" << connection->connectionId()
                << "promoted to controller";
    }
    else
    {
        // Observer  -  inform them of their status
        connection->sendControlStatus();
    }
}

void DaemonServer::onGuiInputReceived(quint8 characterId, const QString &text)
{
    CharacterSession *session = m_registry->session(characterId);
    if (session == nullptr)
    {
        qWarning() << "DaemonServer: input for unknown character" << characterId;
        return;
    }

    // sendInput is thread-safe  -  it's a slot called via queued connection
    // because CharacterSession lives on a different thread
    QMetaObject::invokeMethod(session, "sendInput",
                              Qt::QueuedConnection,
                              Q_ARG(QString, text));
}

void DaemonServer::onGuiProtocolError(GuiConnection *connection,
                                      ProtocolErrorCode code,
                                      const QString &description)
{
    Q_UNUSED(code)
    qWarning() << "DaemonServer: protocol error on connection"
               << connection->connectionId() << ":" << description;
    // Connection will disconnect itself after sending ProtocolError
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void DaemonServer::promoteNewController()
{
    if (m_connections.isEmpty())
    {
        return;
    }

    // Promote the oldest remaining connection (lowest connectionId)
    GuiConnection *oldest = nullptr;
    for (GuiConnection *conn : m_connections)
    {
        if (oldest == nullptr || conn->connectionId() < oldest->connectionId())
        {
            oldest = conn;
        }
    }

    if (oldest != nullptr)
    {
        oldest->sendControlGranted();
        qInfo() << "DaemonServer: connection" << oldest->connectionId()
                << "promoted to controller";
    }
}

void DaemonServer::sendResyncDump(GuiConnection *connection)
{
    const QList<CharacterSession *> sessions = m_registry->allSessions();

    for (CharacterSession *session : sessions)
    {
        sendCharacterInfoDump(connection, session);
    }
}

void DaemonServer::sendCharacterInfoDump(GuiConnection *connection,
                                         CharacterSession *session)
{
    // CharacterInfo: [name_len: 1][name: N][server_profile_id: 1]
    //                [instance_id: 1][status: 1]
    const QByteArray nameUtf8 = session->characterName().toUtf8();
    const quint8 nameLen = static_cast<quint8>(qMin(nameUtf8.size(), 255));

    QByteArray infoPayload;
    infoPayload.append(static_cast<char>(nameLen));
    infoPayload.append(nameUtf8.left(nameLen));
    infoPayload.append(static_cast<char>(0));  // server_profile_id placeholder
    infoPayload.append(static_cast<char>(0));  // instance_id placeholder
    infoPayload.append(static_cast<char>(session->status()));

    connection->sendFrame(MSG_CHARACTER_INFO, session->characterId(),
                          infoPayload);

    // BackbufferBegin with 0 lines  -  backbuffer dump not yet implemented
    QByteArray bbBegin;
    bbBegin.append(static_cast<char>(0)); // num_lines low
    bbBegin.append(static_cast<char>(0)); // num_lines high
    connection->sendFrame(MSG_BACKBUFFER_BEGIN, session->characterId(), bbBegin);

    // BackbufferEnd
    connection->sendFrame(MSG_BACKBUFFER_END, session->characterId(),
                          QByteArray());

    // CharacterInfoEnd
    connection->sendFrame(MSG_CHARACTER_INFO_END, session->characterId(),
                          QByteArray());
}

void DaemonServer::removeConnection(GuiConnection *connection)
{
    m_connections.removeOne(connection);
    connection->deleteLater();
}

GuiConnection *DaemonServer::controllerConnection() const
{
    for (GuiConnection *conn : m_connections)
    {
        if (conn->isController())
        {
            return conn;
        }
    }
    return nullptr;
}
