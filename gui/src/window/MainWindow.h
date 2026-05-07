#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QMap>
#include "connection/DaemonConnectionManager.h"
#include "window/AttentionPanel.h"
#include "window/TiledArea.h"
#include "pane/CharacterPane.h"
#include "dialogs/SettingsDialog.h"

// ---------------------------------------------------------------------------
// MainWindow
//
// Top-level application window. Owns:
//   - DaemonConnectionManager (connection state machine)
//   - TiledArea               (multi-character overview, hidden when empty)
//   - QTabWidget              (docked character panes)
//   - AttentionPanel          (collapsible drawer above tab bar)
//   - QStatusBar              (global connection status)
//
// Responds to connection state changes by locking/unlocking all pane inputs
// and updating the status bar. The single always-available action during any
// connection state is Disconnect.
// ---------------------------------------------------------------------------

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // Menu actions
    void onActionConnect();
    void onActionDisconnect();
    void onActionPathEditor();
    void onActionSettings();
    void onDaemonConnectionSettingsChanged();
    void onActionAbout();

    // Connection manager signals
    void onConnectionStateChanged(DaemonConnectionManager::State newState);
    void onHasControlChanged(bool hasControl);
    void onConnectionError(const QString &description);
    void onFrameReceived(quint8 msgType, quint8 characterId, quint16 sequence,
                         const QByteArray &payload);

    // AttentionPanel
    void onDismissRequested(quint8 characterId, quint16 eventId);

    // Character pane input
    void onInputSubmitted(quint8 characterId, const QString &text);

private:
    void setupMenuBar();
    void setupStatusBar();
    void setupCentralWidget();
    void updateConnectionUi(DaemonConnectionManager::State state);
    void lockAllPanes(bool locked);
    void applyObserverMode(bool observer);

    // Frame handlers
    void handleCharacterInfo(quint8 characterId, const QByteArray &payload);
    void handleCharacterInfoEnd(quint8 characterId);
    void handleStatusChange(quint8 characterId, const QByteArray &payload);
    void handleStyledRun(quint8 characterId, const QByteArray &payload);
    void handleServerMessage(quint8 characterId, const QByteArray &payload);
    void handleAttentionEvent(quint8 characterId, const QByteArray &payload);
    void handleAttentionCleared(quint8 characterId, const QByteArray &payload);
    void handleRoomIdentified(quint8 characterId, const QByteArray &payload);
    void handleEngineStatus(quint8 characterId, const QByteArray &payload);

    // Pane management
    CharacterPane *paneForCharacter(quint8 characterId) const;
    CharacterPane *getOrCreatePane(quint8 characterId,
                                   const QString &characterName);

    // Widgets
    TiledArea      *m_tiledArea      = nullptr;
    QTabWidget     *m_tabWidget      = nullptr;
    AttentionPanel *m_attentionPanel = nullptr;
    QLabel         *m_statusLabel    = nullptr;

    // Menu actions that need enabling/disabling
    QAction *m_connectAction    = nullptr;
    QAction *m_disconnectAction = nullptr;

    // Connection
    DaemonConnectionManager m_connectionManager;
    int                     m_resyncCountDisplayed = 0;

    // Character panes keyed by character_id
    QMap<quint8, CharacterPane *> m_panes;
};
