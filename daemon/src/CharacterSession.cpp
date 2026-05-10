#include "CharacterSession.h"

#include <QDebug>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

CharacterSession::CharacterSession(quint8 characterId,
                                   const QString &characterName,
                                   QObject *parent)
    : QObject(parent)
    , m_characterId(characterId)
    , m_characterName(characterName)
{
    // No socket creation here  -  see init()
}

CharacterSession::~CharacterSession()
{
    // Socket is parented to this object and will be deleted by Qt
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void CharacterSession::setConnectionInfo(const QString &host, quint16 port,
                                         const QString &username,
                                         const QString &password,
                                         bool autoReconnect)
{
    m_mudHost = host;
    m_mudPort = port;
    m_loginUsername = username;
    m_loginPassword = password;
    m_autoReconnect = autoReconnect;
}


void CharacterSession::stop()
{
    m_stopRequested = true;
    m_reconnectTimer.stop();

    if (m_mudSocket != nullptr)
    {
        m_mudSocket->disconnectFromHost();
    }

    // Thread quit is driven by the owner (CharacterRegistry) via
    // QThread::quit() after receiving stopped()
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void CharacterSession::init()
{
    // This runs on the session thread  -  safe to create the socket here
    m_mudSocket = new QTcpSocket(this);

    connect(m_mudSocket, &QTcpSocket::connected,
            this, &CharacterSession::onMudConnected);
    connect(m_mudSocket, &QTcpSocket::disconnected,
            this, &CharacterSession::onMudDisconnected);
    connect(m_mudSocket, &QAbstractSocket::errorOccurred,
            this, &CharacterSession::onMudError);
    connect(m_mudSocket, &QTcpSocket::readyRead,
            this, &CharacterSession::onMudReadyRead);

    // AnsiParser  -  created here on the session thread
    m_ansiParser = new AnsiParser(this);

    connect(m_ansiParser, &AnsiParser::runReady,
            this, &CharacterSession::onRunReady);
    connect(m_ansiParser, &AnsiParser::screenCleared,
            this, &CharacterSession::onScreenCleared);

    qDebug() << "CharacterSession::init() character" << m_characterId
             << m_characterName << "on thread"
             << QThread::currentThread();

    // Reconnect timer used after unexpected disconnects.
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &CharacterSession::connectToMud);

    if (m_mudHost.isEmpty())
    {
        qWarning() << "CharacterSession: no MUD connection info for character"
                   << m_characterId << m_characterName;
        setStatus(CharacterStatus::Disconnected);
        return;
    }

    connectToMud();
}

void CharacterSession::sendInput(const QString &text)
{
    if (m_mudSocket == nullptr)
    {
        return;
    }

    if (m_status != CharacterStatus::Connected)
    {
        return;
    }

    m_mudSocket->write(text.toLocal8Bit());
}

// ---------------------------------------------------------------------------
// MUD socket slots
// ---------------------------------------------------------------------------

void CharacterSession::onMudConnected()
{
    setStatus(CharacterStatus::Connected);
    qDebug() << "CharacterSession: character" << m_characterId
             << "connected to MUD";
}

void CharacterSession::onMudDisconnected()
{
    if (m_stopRequested)
    {
        setStatus(CharacterStatus::Disconnected);
        emit stopped(m_characterId);
        return;
    }

    if (m_ansiParser != nullptr)
    {
        m_ansiParser->reset();
    }

    if (!m_autoReconnect)
    {
        setStatus(CharacterStatus::Disconnected);
        qInfo() << "CharacterSession: character" << m_characterId
                << "disconnected and auto-reconnect disabled";
        return;
    }

    setStatus(CharacterStatus::Reconnecting);
    qDebug() << "CharacterSession: character" << m_characterId
             << "disconnected  -  scheduling reconnect";

    if (!m_reconnectTimer.isActive())
    {
        m_reconnectTimer.start(5000);
    }
}

void CharacterSession::onMudError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    qWarning() << "CharacterSession: character" << m_characterId
               << "MUD socket error:" << m_mudSocket->errorString();
    setStatus(CharacterStatus::Error);
}

void CharacterSession::onMudReadyRead()
{
    const QByteArray data = m_mudSocket->readAll();
    m_ansiParser->onData(data);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void CharacterSession::setStatus(CharacterStatus status)
{
    if (status == m_status)
    {
        return;
    }
    m_status = status;
    emit statusChanged(m_characterId, status);
}

void CharacterSession::connectToMud()
{
    if (m_mudHost.isEmpty())
    {
        qWarning() << "CharacterSession: no MUD host configured for character"
                   << m_characterId;
        return;
    }

    setStatus(CharacterStatus::Connecting);
    m_mudSocket->connectToHost(m_mudHost, m_mudPort);
}

void CharacterSession::onRunReady(StyledRun run)
{
    // Forward to DaemonServer for broadcast to all GUIs.
    // The signal crosses to the main thread via queued connection
    // (CharacterSession lives on its own thread).
    emit runReady(m_characterId, run);
}

void CharacterSession::onScreenCleared()
{
    // TODO: notify GUIs to clear their backbuffers.
    // For now just log  -  the MSG_STYLED_RUN stream already handles
    // display; a dedicated clear message will be added when needed.
    qDebug() << "CharacterSession: character" << m_characterId
             << "screen cleared (ESC[2J)";
}
