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
    // No socket creation here — see init()
}

CharacterSession::~CharacterSession()
{
    // Socket is parented to this object and will be deleted by Qt
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void CharacterSession::stop()
{
    m_stopRequested = true;

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
    // This runs on the session thread — safe to create the socket here
    m_mudSocket = new QTcpSocket(this);

    connect(m_mudSocket, &QTcpSocket::connected,
            this, &CharacterSession::onMudConnected);
    connect(m_mudSocket, &QTcpSocket::disconnected,
            this, &CharacterSession::onMudDisconnected);
    connect(m_mudSocket, &QAbstractSocket::errorOccurred,
            this, &CharacterSession::onMudError);
    connect(m_mudSocket, &QTcpSocket::readyRead,
            this, &CharacterSession::onMudReadyRead);

    qDebug() << "CharacterSession::init() character" << m_characterId
             << m_characterName << "on thread"
             << QThread::currentThread();

    // TODO: load MUD connection parameters from database and connect
    // connectToMud();
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

    // Unexpected disconnect — will reconnect
    setStatus(CharacterStatus::Reconnecting);
    qDebug() << "CharacterSession: character" << m_characterId
             << "disconnected — will reconnect";

    // TODO: reconnect timer
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

    // TODO: feed data to AnsiParser
    // For now just log the byte count
    qDebug() << "CharacterSession: character" << m_characterId
             << "received" << data.size() << "bytes from MUD";
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
