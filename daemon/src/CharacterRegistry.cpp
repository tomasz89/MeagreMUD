#include "CharacterRegistry.h"

#include <QDebug>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

CharacterRegistry::CharacterRegistry(QObject *parent)
    : QObject(parent)
{
}

CharacterRegistry::~CharacterRegistry()
{
    // Sessions and threads should have been stopped via stopAll() before
    // destruction. Force cleanup of anything remaining.
    for (auto &entry : m_sessions)
    {
        teardownSession(entry);
    }
    m_sessions.clear();
}

// ---------------------------------------------------------------------------
// Session management
// ---------------------------------------------------------------------------

CharacterSession *CharacterRegistry::addSession(quint8 characterId,
                                                const QString &characterName,
                                                int characterDbId,
                                                const QString &mudHost,
                                                quint16 mudPort,
                                                const QString &loginUsername,
                                                const QString &loginPassword,
                                                bool autoReconnect,
                                                quint8 serverProfileId,
                                                quint8 instanceId)
{
    if (m_sessions.contains(characterId))
    {
        qWarning() << "CharacterRegistry: session already exists for character"
                   << characterId;
        return m_sessions.value(characterId).session;
    }

    auto *session = new CharacterSession(characterId, characterName);
    auto *thread  = new QThread(this);

    session->setDatabaseId(characterDbId);
    session->setConnectionInfo(mudHost, mudPort, loginUsername,
                               loginPassword, autoReconnect);
    session->setServerProfileId(serverProfileId);
    session->setInstanceId(instanceId);

    thread->setObjectName(
        QStringLiteral("SessionThread-%1-%2")
            .arg(characterId)
            .arg(characterName));

    // Move session to its thread  -  must happen before the thread starts
    session->moveToThread(thread);

    // init() called via queued connection once the thread event loop starts
    connect(thread,  &QThread::started,
            session, &CharacterSession::init,
            Qt::QueuedConnection);

    // Forward session signals to registry signals (automatic queued connection
    // because session and registry are on different threads)
    connect(session, &CharacterSession::runReady,
            this,    &CharacterRegistry::runReady);
    connect(session, &CharacterSession::statusChanged,
            this,    &CharacterRegistry::statusChanged);
    connect(session, &CharacterSession::roomIdentified,
            this,    &CharacterRegistry::roomIdentified);
    connect(session, &CharacterSession::stopped,
            this,    &CharacterRegistry::onSessionStopped);

    // Clean up the session object when the thread finishes
    connect(thread,  &QThread::finished,
            session, &QObject::deleteLater);

    SessionEntry entry;
    entry.session = session;
    entry.thread  = thread;
    m_sessions.insert(characterId, entry);

    startSession(entry);

    qDebug() << "CharacterRegistry: started session for character"
             << characterId << characterName;

    return session;
}

CharacterSession *CharacterRegistry::session(quint8 characterId) const
{
    return m_sessions.value(characterId).session;
}

QList<CharacterSession *> CharacterRegistry::allSessions() const
{
    QList<CharacterSession *> result;
    result.reserve(m_sessions.size());
    for (const auto &entry : m_sessions)
    {
        result.append(entry.session);
    }
    return result;
}

void CharacterRegistry::stopAll()
{
    if (m_sessions.isEmpty())
    {
        emit allStopped();
        return;
    }

    m_pendingStops = m_sessions.size();

    for (auto &entry : m_sessions)
    {
        // stop() is thread-safe  -  it sets a flag and disconnects the socket.
        // The session emits stopped() from its own thread when done,
        // which arrives here via queued connection.
        entry.session->stop();
    }
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void CharacterRegistry::onSessionStopped(quint8 characterId)
{
    qDebug() << "CharacterRegistry: session stopped for character" << characterId;

    if (m_sessions.contains(characterId))
    {
        SessionEntry &entry = m_sessions[characterId];
        entry.thread->quit();
        // thread->finished will deleteLater the session
        // We do not wait here  -  the thread cleans up asynchronously
    }

    m_pendingStops--;
    if (m_pendingStops <= 0)
    {
        m_pendingStops = 0;
        emit allStopped();
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void CharacterRegistry::startSession(SessionEntry &entry)
{
    entry.thread->start();
}

void CharacterRegistry::teardownSession(SessionEntry &entry)
{
    if (entry.thread == nullptr)
    {
        return;
    }

    if (entry.thread->isRunning())
    {
        entry.thread->quit();
        entry.thread->wait(3000); // 3 second timeout
    }

    // Thread deletes session via deleteLater on finished signal.
    // Thread itself is parented to registry and deleted with it.
}
