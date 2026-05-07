#pragma once

#include <QObject>
#include <QTcpServer>
#include <QList>
#include "GuiConnection.h"
#include "CharacterRegistry.h"

// ---------------------------------------------------------------------------
// DaemonServer
//
// Lives on the main thread. Owns the TCP listener and all GuiConnections.
// Coordinates with CharacterRegistry to route data between sessions and GUIs.
//
// Controller arbitration:
//   - First GUI to complete resync becomes controller
//   - On controller disconnect, the oldest remaining GUI (lowest connectionId)
//     is promoted automatically
//   - All GUIs receive all output regardless of controller status
//
// Resync flow (per GUI):
//   1. GuiConnection receives ClientHello + ResyncRequest
//   2. GuiConnection emits resyncRequested()
//   3. DaemonServer sends ResyncAck to that GUI
//   4. DaemonServer sends CharacterInfo + backbuffer dump for each session
//   5. DaemonServer marks resync complete on the GuiConnection
//   6. If no controller exists, promote this GUI to controller
// ---------------------------------------------------------------------------

class DaemonServer : public QObject
{
    Q_OBJECT

public:
    explicit DaemonServer(CharacterRegistry *registry,
                          QObject *parent = nullptr);
    ~DaemonServer() override;

    // Start listening. Returns true on success.
    bool listen(const QHostAddress &address, quint16 port);

    void stopListening();

    bool isListening() const;
    quint16 serverPort() const;

    int guiConnectionCount() const { return m_connections.size(); }

public slots:
    // Broadcast a StyledRun to all connected GUIs.
    void broadcastStyledRun(quint8 characterId, StyledRun run);

    // Broadcast a status change to all connected GUIs.
    void broadcastStatusChange(quint8 characterId, CharacterStatus status);

    // Broadcast a room identification to all connected GUIs.
    void broadcastRoomIdentified(quint8 characterId, quint16 roomId,
                                 const QString &roomName);

private slots:
    void onNewConnection();
    void onGuiDisconnected(GuiConnection *connection);
    void onGuiResyncRequested(GuiConnection *connection);
    void onGuiInputReceived(quint8 characterId, const QString &text);
    void onGuiProtocolError(GuiConnection *connection,
                            ProtocolErrorCode code,
                            const QString &description);

private:
    void promoteNewController();
    void sendResyncDump(GuiConnection *connection);
    void sendCharacterInfoDump(GuiConnection *connection,
                               CharacterSession *session);
    void removeConnection(GuiConnection *connection);

    GuiConnection *controllerConnection() const;

    QTcpServer          m_server;
    CharacterRegistry  *m_registry;
    QList<GuiConnection *> m_connections;
    int                 m_nextConnectionId = 1;
};
