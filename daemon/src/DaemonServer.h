#pragma once

/**
 * @file DaemonServer.h
 * @brief Top-level TCP server and GUI connection coordinator for the daemon.
 */

#include <QObject>
#include <QTcpServer>
#include <QList>
#include "GuiConnection.h"
#include "CharacterRegistry.h"

/**
 * @brief Top-level TCP server that owns all GuiConnection instances and
 *        coordinates data routing between character sessions and connected GUIs.
 *
 * DaemonServer lives on the main thread alongside CharacterRegistry. It
 * listens for incoming GUI TCP connections, manages the KVM-like controller
 * arbitration model, and broadcasts session output to all connected GUIs.
 *
 * ## Controller arbitration
 * - The first GUI to complete resync becomes the controller.
 * - On controller disconnect, the oldest remaining GUI (lowest connectionId)
 *   is promoted automatically.
 * - All GUIs receive all output regardless of controller status. Only the
 *   controller's input frames are acted upon.
 *
 * ## Resync flow (per GUI)
 * -# GuiConnection receives ClientHello and sends ServerHello.
 * -# GuiConnection receives ResyncRequest and emits resyncRequested().
 * -# DaemonServer sends ResyncAck to the GUI.
 * -# DaemonServer sends CharacterInfo + backbuffer dump for every session.
 * -# DaemonServer marks resync complete on the GuiConnection.
 * -# If no controller exists, the GUI is promoted to controller.
 *
 * @see GuiConnection
 * @see CharacterRegistry
 */
class DaemonServer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct the server.
     * @param registry  CharacterRegistry that owns all character sessions.
     *                  Must outlive DaemonServer. Not transferred.
     * @param parent    Qt parent object.
     */
    explicit DaemonServer(CharacterRegistry *registry,
                          QObject *parent = nullptr);

    /// Sends Goodbye to all connected GUIs and closes the listener.
    ~DaemonServer() override;

    /**
     * @brief Start listening for GUI connections.
     * @param address  Bind address (e.g. QHostAddress::LocalHost).
     * @param port     TCP port to listen on.
     * @return @c true on success; @c false if the port is unavailable.
     */
    bool listen(const QHostAddress &address, quint16 port);

    /// Stop accepting new connections. Existing connections are unaffected.
    void stopListening();

    /// @return @c true if the server is currently listening.
    bool isListening() const;

    /// @return The port actually bound (useful when port 0 is requested).
    quint16 serverPort() const;

    /// @return Number of currently connected GUI clients.
    int guiConnectionCount() const { return m_connections.size(); }

public slots:
    /**
     * @brief Broadcast a parsed StyledRun to every connected GUI.
     *
     * Connected from CharacterRegistry::runReady() via automatic queued
     * connection because CharacterSession emits from its own thread.
     *
     * @param characterId  Character that produced the run.
     * @param run          The styled text run to forward.
     */
    void broadcastStyledRun(quint8 characterId, StyledRun run);

    /**
     * @brief Broadcast a character status change to every connected GUI.
     * @param characterId  Affected character.
     * @param status       New connection status.
     */
    void broadcastStatusChange(quint8 characterId, CharacterStatus status);

    /**
     * @brief Broadcast a room identification event to every connected GUI.
     * @param characterId  Character whose room was identified.
     * @param roomId       Database room ID (0 if unknown).
     * @param roomName     Human-readable room name.
     */
    void broadcastRoomIdentified(quint8 characterId, quint16 roomId,
                                 const QString &roomName);

private slots:
    /// Called by QTcpServer when a new GUI client connects.
    void onNewConnection();

    /**
     * @brief Called when a GuiConnection's socket closes.
     *
     * Removes the connection from the list. If it was the controller,
     * promotes the oldest remaining GUI.
     *
     * @param connection  The disconnected GUI (will be deleted).
     */
    void onGuiDisconnected(GuiConnection *connection);

    /**
     * @brief Called when a GUI requests a full state resync.
     *
     * Sends ResyncAck, performs the state dump, marks resync complete,
     * and promotes to controller if no controller exists.
     *
     * @param connection  The GUI requesting the resync.
     */
    void onGuiResyncRequested(GuiConnection *connection);

    /**
     * @brief Called when the controller GUI submits input text.
     * @param characterId  Target character.
     * @param text         Input text (already has \\r\\n appended by the GUI).
     */
    void onGuiInputReceived(quint8 characterId, const QString &text);

    /**
     * @brief Called when GuiConnection detects a protocol error.
     * @param connection   The offending GUI connection.
     * @param code         Error code.
     * @param description  Human-readable error description.
     */
    void onGuiProtocolError(GuiConnection *connection,
                            ProtocolErrorCode code,
                            const QString &description);

private:
    /// Promote the oldest remaining GuiConnection to controller.
    void promoteNewController();

    /**
     * @brief Send the complete state dump to a newly syncing GUI.
     * @param connection  GUI to receive the dump.
     */
    void sendResyncDump(GuiConnection *connection);

    /**
     * @brief Send CharacterInfo + backbuffer for one character session.
     * @param connection  GUI to receive the dump.
     * @param session     Character session to describe.
     */
    void sendCharacterInfoDump(GuiConnection *connection,
                               CharacterSession *session);

    /**
     * @brief Remove and schedule deletion of a GuiConnection.
     * @param connection  Connection to remove.
     */
    void removeConnection(GuiConnection *connection);

    /**
     * @brief Find the current controller GuiConnection.
     * @return Pointer to the controller, or @c nullptr if none.
     */
    GuiConnection *controllerConnection() const;

    QTcpServer             m_server;         ///< Qt TCP listener.
    CharacterRegistry     *m_registry;       ///< Owned externally; not deleted here.
    QList<GuiConnection *> m_connections;    ///< All currently connected GUIs.
    int                    m_nextConnectionId = 1; ///< Monotonically increasing connection ID.
};
