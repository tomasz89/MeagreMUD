#pragma once

/**
 * @file MainWindow.h
 * @brief Top-level application window for the MeagreMUD GUI.
 */

#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QMap>
#include "connection/DaemonConnectionManager.h"
#include "window/AttentionPanel.h"
#include "window/TiledArea.h"
#include "pane/CharacterPane.h"
#include "dialogs/SettingsDialog.h"

/**
 * @brief Top-level application window.
 *
 * Owns the DaemonConnectionManager, all CharacterPane instances, the
 * AttentionPanel, and the TiledArea. Responds to connection state changes
 * by locking/unlocking pane inputs and updating the status bar.
 *
 * ## Layout (top to bottom)
 * - QMenuBar
 * - TiledArea (hidden when no tiles exist)
 * - AttentionPanel (collapsible drawer)
 * - QTabWidget (docked character panes)
 * - QStatusBar (global connection status)
 *
 * ## Input locking policy
 * All pane inputs are locked from connect until ResyncAck is received
 * (Disconnected, Connecting, Syncing states). In the Connected state,
 * observer GUIs have inputs locked; the controller GUI has inputs unlocked.
 * The Disconnect action is always available regardless of state.
 *
 * ## Frame routing
 * frameReceived() is connected to DaemonConnectionManager and dispatches
 * each incoming daemon frame to a typed handler method. Unrecognised message
 * types are logged and discarded.
 *
 * @see DaemonConnectionManager, CharacterPane, AttentionPanel
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Construct the main window.
     * @param parent Qt parent widget (usually nullptr for a top-level window).
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /// Disconnects cleanly from the daemon before Qt tears down the socket.
    ~MainWindow() override;

private slots:
    /// Open SettingsDialog at the Daemon Connection tab if no host is saved,
    /// otherwise connect immediately using saved settings.
    void onActionConnect();

    /// Send Goodbye and disconnect from the daemon.
    void onActionDisconnect();

    /// Open the PathEditor window (stub until PathEditor is implemented).
    void onActionPathEditor();

    /// Open the SettingsDialog.
    void onActionSettings();

    /// Called when ConnectionDialog requests connect with a profile.
    void onConnectRequested(const QString &profile);

    /// Called when ConnectionDialog requests disconnect.
    void onDisconnectRequested();

    /// Show the About dialog.
    void onActionAbout();

    /**
     * @brief React to DaemonConnectionManager state changes.
     *
     * Locks/unlocks panes and updates the status bar label and menu actions.
     *
     * @param newState  The new connection state.
     */
    void onConnectionStateChanged(DaemonConnectionManager::State newState);

    /**
     * @brief React to controller/observer status changes.
     * @param hasControl @c true if this GUI is now the controller.
     */
    void onHasControlChanged(bool hasControl);

    /**
     * @brief Display a connection error in the status bar.
     * @param description Human-readable error description.
     */
    void onConnectionError(const QString &description);

    /**
     * @brief Dispatch an incoming daemon frame to the appropriate handler.
     * @param msgType     Message type byte.
     * @param characterId Source character (or CHARACTER_ID_DAEMON).
     * @param sequence    Frame sequence number.
     * @param payload     Raw payload bytes (header stripped).
     */
    void onFrameReceived(quint8 msgType, quint8 characterId, quint16 sequence,
                         const QByteArray &payload);

    /**
     * @brief Forward an attention event dismissal to the daemon.
     * @param characterId  Owning character.
     * @param eventId      Event to dismiss.
     */
    void onDismissRequested(quint8 characterId, quint16 eventId);

    /**
     * @brief Forward user input to the daemon via DaemonConnectionManager.
     *
     * Only acts if the GUI has control. Text already has @c \\r\\n appended.
     *
     * @param characterId  Target character.
     * @param text         Input text including trailing \\r\\n.
     */
    void onInputSubmitted(quint8 characterId, const QString &text);

private:
    void setupMenuBar();
    void setupStatusBar();
    void setupCentralWidget();
    void updateConnectionUi(DaemonConnectionManager::State state);
    void lockAllPanes(bool locked);
    void applyObserverMode(bool observer);

    /// @name Frame handlers
    /// Called from onFrameReceived() for each recognised message type.
    /// @{
    void handleCharacterInfo(quint8 characterId, const QByteArray &payload);
    void handleCharacterInfoEnd(quint8 characterId);
    void handleServerHello(quint8 characterId, const QByteArray &payload);
    void handleStatusChange(quint8 characterId, const QByteArray &payload);
    void handleStyledRun(quint8 characterId, const QByteArray &payload);
    void handleServerMessage(quint8 characterId, const QByteArray &payload);
    void handleAttentionEvent(quint8 characterId, const QByteArray &payload);
    void handleAttentionCleared(quint8 characterId, const QByteArray &payload);
    void handleRoomIdentified(quint8 characterId, const QByteArray &payload);
    void handleEngineStatus(quint8 characterId, const QByteArray &payload);
    /// @}

    /**
     * @brief Look up the CharacterPane for a given character ID.
     * @param characterId  Character to look up.
     * @return Pointer to the pane, or @c nullptr if not found.
     */
    CharacterPane *paneForCharacter(quint8 characterId) const;

    /**
     * @brief Return the pane for a character, creating it if necessary.
     *
     * New panes are added as tabs in the QTabWidget and inherit the current
     * lock and observer state.
     *
     * @param characterId    Character ID from the daemon.
     * @param characterName  Display name for the tab and status bar.
     * @return               The existing or newly created CharacterPane.
     */
    CharacterPane *getOrCreatePane(quint8 characterId,
                                   const QString &characterName);

    TiledArea      *m_tiledArea      = nullptr; ///< Multi-character tile grid.
    QTabWidget     *m_tabWidget      = nullptr; ///< Docked character pane tabs.
    AttentionPanel *m_attentionPanel = nullptr; ///< Collapsible event drawer.
    QLabel         *m_statusLabel    = nullptr; ///< Global status bar label.

    QAction *m_connectAction     = nullptr; ///< Enabled only when Disconnected.
    QAction *m_disconnectAction  = nullptr; ///< Enabled only when Connected.
    QAction *m_pathEditorAction  = nullptr; ///< Enabled only when Connected.
    QAction *m_settingsAction    = nullptr; ///< Enabled only when Connected.

    DaemonConnectionManager m_connectionManager; ///< Owns the daemon TCP socket.
    int m_resyncCountDisplayed = 0; ///< Last resync count shown to the user.

    QMap<quint8, CharacterPane *> m_panes; ///< Character panes keyed by character_id.
};
