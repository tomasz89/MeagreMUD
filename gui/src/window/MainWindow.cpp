#include "window/MainWindow.h"

#include <QApplication>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QSettings>
#include <QDebug>

#include "dialogs/ConnectionDialog.h"
#include "toolbar/ToolbarManager.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("MeagreMUD"));
    setMinimumSize(800, 600);
    resize(1200, 800);

    setupMenuBar();
    setupCentralWidget();
    setupStatusBar();

    // Toolbar
    m_toolbarManager = new ToolbarManager(this, this);
    addToolBar(Qt::TopToolBarArea, m_toolbarManager->toolbar());
    connect(m_toolbarManager, &ToolbarManager::settingsRequested,
            this, &MainWindow::onActionSettings);
    // Apply initial pre-connection state with File menu actions
    m_toolbarManager->applyPreConnectionState(
        m_connectAction,
        m_quickConnectAction,
        m_autoConnectAction,
        m_disconnectAction);

    // Wire up connection manager
    connect(&m_connectionManager, &DaemonConnectionManager::stateChanged,
            this, &MainWindow::onConnectionStateChanged);
    connect(&m_connectionManager, &DaemonConnectionManager::hasControlChanged,
            this, &MainWindow::onHasControlChanged);
    connect(&m_connectionManager, &DaemonConnectionManager::errorOccurred,
            this, &MainWindow::onConnectionError);
    connect(&m_connectionManager, &DaemonConnectionManager::frameReceived,
            this, &MainWindow::onFrameReceived);

    // Start in disconnected state
    updateConnectionUi(DaemonConnectionManager::State::Disconnected);

    // Wire retry timer  -  single-shot, reconnects on timeout
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, [this]()
    {
        if (!m_retryProfile.isEmpty()
            && m_connectionManager.state() == DaemonConnectionManager::State::Disconnected)
        {
            statusBar()->showMessage(
                QStringLiteral("[MeagreMUD] Retrying connection to %1...")
                    .arg(m_retryProfile));
            m_countdownTimer.stop();
            onConnectRequested(m_retryProfile);
        }
    });


    // Wire countdown timer  -  ticks every second to update the status bar
    m_countdownTimer.setInterval(1000);
    m_countdownTimer.setSingleShot(false);
    connect(&m_countdownTimer, &QTimer::timeout, this, [this]()
    {
        m_countdownSeconds--;
        if (m_countdownSeconds > 0)
        {
            statusBar()->showMessage(
                QStringLiteral("[MeagreMUD] Connection failed - retrying in %1s...")
                    .arg(m_countdownSeconds),
                1500);
        }
    });
    // Auto-connect if enabled and a last profile exists
    if (loadAutoConnect())
    {
        const QString profile = ConnectionDialog::lastUsedProfile();
        if (!profile.isEmpty())
        {
            // Use a single-shot timer so the window is fully shown first
            QTimer::singleShot(0, this, [this, profile]()
            {
                onConnectRequested(profile);
            });
        }
    }
}

MainWindow::~MainWindow()
{
    // Disconnect cleanly before Qt tears down the socket and event loop.
    // DaemonConnectionManager is a value member  -  its destructor would
    // otherwise call sendGoodbye() during object teardown when the
    // underlying socket may already be in an invalid state.
    m_connectionManager.disconnectFromDaemon();
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void MainWindow::setupMenuBar()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));

    m_connectAction = fileMenu->addAction(QStringLiteral("&Connection..."));
    connect(m_connectAction, &QAction::triggered,
            this, &MainWindow::onActionConnect);

    m_quickConnectAction = fileMenu->addAction(QStringLiteral("Quick &Connect"));
    m_quickConnectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connect(m_quickConnectAction, &QAction::triggered,
            this, &MainWindow::onActionQuickConnect);

    m_disconnectAction = fileMenu->addAction(QStringLiteral("&Disconnect"));
    connect(m_disconnectAction, &QAction::triggered,
            this, &MainWindow::onActionDisconnect);

    fileMenu->addSeparator();

    m_autoConnectAction = fileMenu->addAction(QStringLiteral("Auto-connect on launch"));
    m_autoConnectAction->setCheckable(true);
    m_autoConnectAction->setChecked(loadAutoConnect());
    connect(m_autoConnectAction, &QAction::toggled,
            this, &MainWindow::onAutoConnectToggled);

    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction(QStringLiteral("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered,
            qApp, &QApplication::quit);

    // Tools menu
    QMenu *toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));

    m_pathEditorAction = toolsMenu->addAction(
        QStringLiteral("&Path Editor..."));
    connect(m_pathEditorAction, &QAction::triggered,
            this, &MainWindow::onActionPathEditor);

    toolsMenu->addSeparator();

    m_settingsAction = toolsMenu->addAction(
        QStringLiteral("&Settings..."));
    m_settingsAction->setShortcut(QKeySequence::Preferences);
    connect(m_settingsAction, &QAction::triggered,
            this, &MainWindow::onActionSettings);

    // Help menu
    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));

    QAction *aboutAction = helpMenu->addAction(QStringLiteral("&About MeagreMUD"));
    connect(aboutAction, &QAction::triggered,
            this, &MainWindow::onActionAbout);
}

void MainWindow::setupCentralWidget()
{
    auto *central = new QWidget(this);
    auto *layout  = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Tiled overview  -  hidden until panes are added to it
    m_tiledArea = new TiledArea(central);
    layout->addWidget(m_tiledArea);

    // Attention panel drawer  -  sits above tab bar
    m_attentionPanel = new AttentionPanel(central);
    layout->addWidget(m_attentionPanel);
    connect(m_attentionPanel, &AttentionPanel::dismissRequested,
            this, &MainWindow::onDismissRequested);

    // Tab widget  -  one tab per docked character
    m_tabWidget = new QTabWidget(central);
    m_tabWidget->setTabsClosable(false);
    m_tabWidget->setMovable(true);
    layout->addWidget(m_tabWidget, 1);

    setCentralWidget(central);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel(QStringLiteral("Disconnected"), this);
    statusBar()->addWidget(m_statusLabel, 1);
}

// ---------------------------------------------------------------------------
// Menu action slots
// ---------------------------------------------------------------------------

void MainWindow::onActionConnect()
{
    // Always open the dialog regardless of state. The dialog's Connect
    // button is disabled when already connected; its Disconnect button
    // lets the user abort a stuck connecting/syncing attempt.
    ConnectionDialog dlg(m_connectionManager.state(), this);
    connect(&dlg, &ConnectionDialog::connectRequested,
            this, &MainWindow::onConnectRequested);
    connect(&dlg, &ConnectionDialog::disconnectRequested,
            this, &MainWindow::onDisconnectRequested);
    dlg.exec();
}

void MainWindow::onConnectRequested(const QString &profile)
{
    m_userDisconnected = false;
    m_connectionManager.connectToDaemon(
        SettingsDialog::savedHost(profile),
        SettingsDialog::savedPort(profile));
}

void MainWindow::onDisconnectRequested()
{
    m_connectionManager.disconnectFromDaemon();
}
void MainWindow::onActionDisconnect()
{
    m_userDisconnected = true;
    cancelRetry();
    m_connectionManager.disconnectFromDaemon();
}

void MainWindow::onActionQuickConnect()
{
    const QString profile = ConnectionDialog::lastUsedProfile();
    if (profile.isEmpty())
    {
        return;
    }
    onConnectRequested(profile);
}

void MainWindow::onAutoConnectToggled(bool enabled)
{
    saveAutoConnect(enabled);
    if (!enabled)
    {
        cancelRetry();
    }
}

void MainWindow::scheduleRetry(const QString &profile)
{
    m_retryProfile = profile;
    m_countdownSeconds = RETRY_INTERVAL_MS / 1000;
    m_retryTimer.start(RETRY_INTERVAL_MS);
    m_countdownTimer.start();
    statusBar()->showMessage(
        QStringLiteral("[MeagreMUD] Connection failed - retrying in %1s...")
            .arg(m_countdownSeconds),
        1500);
}

void MainWindow::cancelRetry()
{
    m_retryProfile.clear();
    m_retryTimer.stop();
    m_countdownTimer.stop();
    m_countdownSeconds = 0;
}

bool MainWindow::loadAutoConnect() const
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("MeagreMUD"), QStringLiteral("MeagreMUD"));
    s.beginGroup(QStringLiteral("gui"));
    const bool result = s.value(QStringLiteral("auto_connect"), false).toBool();
    s.endGroup();
    return result;
}

void MainWindow::saveAutoConnect(bool enabled)
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("MeagreMUD"), QStringLiteral("MeagreMUD"));
    s.beginGroup(QStringLiteral("gui"));
    s.setValue(QStringLiteral("auto_connect"), enabled);
    s.endGroup();
    s.sync();
}

void MainWindow::onActionPathEditor()
{
    // PathEditor window  -  implemented separately
    QMessageBox::information(this, QStringLiteral("Path Editor"),
                             QStringLiteral("Path Editor not yet implemented."));
}

void MainWindow::onActionSettings()
{
    SettingsDialog dlg(this);
    dlg.exec();
}

void MainWindow::onActionAbout()
{
    QMessageBox::about(this,
        QStringLiteral("About MeagreMUD"),
        QStringLiteral(
            "<b>MeagreMUD</b> v0.1.0<br>"
            "Open source MegaMUD clone.<br>"
            "GPL-3.0  -  targeting GreaterMUD / MajorMUD servers."));
}

// ---------------------------------------------------------------------------
// Connection manager signal handlers
// ---------------------------------------------------------------------------

void MainWindow::onConnectionStateChanged(DaemonConnectionManager::State newState)
{
    updateConnectionUi(newState);

    switch (newState)
    {
        case DaemonConnectionManager::State::Disconnected:
            // Dim all panes but preserve their content
            lockAllPanes(true);
            if (m_toolbarManager != nullptr)
            {
                m_toolbarManager->applyPreConnectionState(
                    m_connectAction,
                    m_quickConnectAction,
                    m_autoConnectAction,
                    m_disconnectAction);
            }

            // Schedule retry if auto-connect is on and disconnect wasn't user-initiated
            if (loadAutoConnect() && !m_userDisconnected)
            {
                const QString profile = ConnectionDialog::lastUsedProfile();
                if (!profile.isEmpty())
                {
                    scheduleRetry(profile);
                }
            }
            else
            {
                cancelRetry();
            }
            break;

        case DaemonConnectionManager::State::Connecting:
            lockAllPanes(true);
            statusBar()->clearMessage();
            break;

        case DaemonConnectionManager::State::Syncing:
            // Lock inputs until ResyncAck  -  panes dimmed, only Disconnect available
            lockAllPanes(true);

            if (m_toolbarManager != nullptr)
            {
                m_toolbarManager->applyPreConnectionState(
                    m_connectAction,
                    m_quickConnectAction,
                    m_autoConnectAction,
                    m_disconnectAction);
            }

            if (m_connectionManager.resyncCount() > 0)
            {
                const QString msg = QStringLiteral("[MeagreMUD] Protocol resync #%1")
                    .arg(m_connectionManager.resyncCount());
                qDebug() << msg;
                // TODO: display in active character pane as a server message
            }
            break;

        case DaemonConnectionManager::State::Connected:
            // Unlock inputs  -  observer mode governed separately by hasControl
            lockAllPanes(false);

            // Clear any transient status bar messages (e.g. "Retrying...")
            // so the permanent status label is visible again
            cancelRetry();
            statusBar()->clearMessage();

            if (m_toolbarManager != nullptr)
            {
                m_toolbarManager->applyPostConnectionState(
                    ToolbarManager::defaultButtonOrder());
                QAction *sa = m_toolbarManager->action(
                    QString::fromLatin1(ToolbarManager::ID_SETTINGS));
                if (sa != nullptr) { sa->setEnabled(true); }
            }
            break;
    }
}

void MainWindow::onHasControlChanged(bool hasControl)
{
    applyObserverMode(!hasControl);
}

void MainWindow::onConnectionError(const QString &description)
{
    statusBar()->showMessage(
        QStringLiteral("[MeagreMUD] Error: %1").arg(description), 5000);
}

void MainWindow::onFrameReceived(quint8 msgType, quint8 characterId,
                                 quint16 sequence, const QByteArray &payload)
{
    Q_UNUSED(sequence)

    switch (msgType)
    {
        case MSG_SERVER_HELLO:
            handleServerHello(characterId, payload);
            break;

        case MSG_CHARACTER_INFO:
            handleCharacterInfo(characterId, payload);
            break;

        case MSG_CHARACTER_INFO_END:
            handleCharacterInfoEnd(characterId);
            break;

        case MSG_STATUS_CHANGE:
            handleStatusChange(characterId, payload);
            break;

        case MSG_STYLED_RUN:
            handleStyledRun(characterId, payload);
            break;

        case MSG_SERVER_MESSAGE:
            handleServerMessage(characterId, payload);
            break;

        case MSG_ATTENTION_EVENT:
            handleAttentionEvent(characterId, payload);
            break;

        case MSG_ATTENTION_CLEARED:
            handleAttentionCleared(characterId, payload);
            break;


        case MSG_ROOM_IDENTIFIED:
            handleRoomIdentified(characterId, payload);
            break;

        case MSG_ENGINE_STATUS:
            handleEngineStatus(characterId, payload);
            break;

        case MSG_PING:
            // Should be handled by DaemonConnectionManager  -  ignore if it leaks through
            return;

        case MSG_PONG:
            // Keepalive  -  no action needed
            return;

        case MSG_GOODBYE:
            // Daemon disconnecting  -  DaemonConnectionManager handles state transition
            return;

        default:
            qDebug() << "MainWindow: unhandled message type"
                     << Qt::hex << msgType
                     << "for character" << characterId;
            break;
    }
}

// ---------------------------------------------------------------------------
// Frame handlers
// ---------------------------------------------------------------------------

void MainWindow::handleCharacterInfo(quint8 characterId,
                                     const QByteArray &payload)
{
    if (payload.size() < 2)
    {
        return;
    }

    const int nameLen = static_cast<quint8>(payload[0]);
    if (payload.size() < 1 + nameLen)
    {
        return;
    }

    const QString name = QString::fromUtf8(payload.mid(1, nameLen));
    getOrCreatePane(characterId, name);
}

void MainWindow::handleCharacterInfoEnd(quint8 characterId)
{
    Q_UNUSED(characterId)
    // All CharacterInfo messages for the resync dump have been received.
    // Nothing to do at this level  -  panes are already created.
}

void MainWindow::handleServerHello(quint8 characterId,
                                   const QByteArray &payload)
{
    Q_UNUSED(characterId)
    if (payload.size() < 3)
    {
        qWarning() << "MainWindow: malformed MSG_SERVER_HELLO payload";
        return;
    }

    const quint8 protocolVersion = static_cast<quint8>(payload[0]);
    const quint8 serverFlags     = static_cast<quint8>(payload[1]);
    const quint8 numCharacters   = static_cast<quint8>(payload[2]);

    qDebug() << "MainWindow: ServerHello received  -  protocol v" << protocolVersion
             << ", flags" << Qt::hex << serverFlags
             << ", characters:" << numCharacters;
}

void MainWindow::handleStatusChange(quint8 characterId,
                                    const QByteArray &payload)
{
    CharacterPane *pane = paneForCharacter(characterId);
    if (pane == nullptr || payload.isEmpty())
    {
        return;
    }
    const auto status = static_cast<CharacterStatus>(
        static_cast<quint8>(payload[0]));
    pane->setCharacterStatus(status);
}

void MainWindow::handleStyledRun(quint8 characterId,
                                 const QByteArray &payload)
{
    // Wire format: [fg: 1][bg: 1][bold: 1][text_len: 2][text: N (UTF-8)]
    if (payload.size() < 5)
    {
        return;
    }

    CharacterPane *pane = paneForCharacter(characterId);
    if (pane == nullptr)
    {
        return;
    }

    StyledRun run;
    run.fg   = static_cast<quint8>(payload[0]);
    run.bg   = static_cast<quint8>(payload[1]);
    run.bold = (static_cast<quint8>(payload[2]) != 0);

    const quint16 textLen = static_cast<quint8>(payload[3])
                          | (static_cast<quint8>(payload[4]) << 8u);

    if (payload.size() < 5 + textLen)
    {
        return;
    }

    run.text = QString::fromUtf8(payload.mid(5, textLen));
    pane->appendRun(run);
}

void MainWindow::handleServerMessage(quint8 characterId,
                                     const QByteArray &payload)
{
    Q_UNUSED(payload)
    CharacterPane *pane = paneForCharacter(characterId);
    if (pane == nullptr)
    {
        return;
    }
    // Rendered with [MeagreMUD] prefix in blue  -  TerminalWidget handles this
}

void MainWindow::handleAttentionEvent(quint8 characterId,
                                      const QByteArray &payload)
{
    if (payload.size() < 5)
    {
        return;
    }

    const quint16 eventId   = static_cast<quint8>(payload[0])
                            | (static_cast<quint8>(payload[1]) << 8u);
    const quint8  eventType = static_cast<quint8>(payload[2]);
    const quint16 textLen   = static_cast<quint8>(payload[3])
                            | (static_cast<quint8>(payload[4]) << 8u);

    if (payload.size() < 5 + textLen)
    {
        return;
    }

    const QString text = QString::fromUtf8(payload.mid(5, textLen));
    m_attentionPanel->addEvent(characterId, eventId, eventType, text);
}

void MainWindow::handleAttentionCleared(quint8 characterId,
                                        const QByteArray &payload)
{
    if (payload.size() < 2)
    {
        return;
    }

    const quint16 eventId = static_cast<quint8>(payload[0])
                          | (static_cast<quint8>(payload[1]) << 8u);
    m_attentionPanel->removeEvent(characterId, eventId);
}

// ---------------------------------------------------------------------------
// AttentionPanel slot
// ---------------------------------------------------------------------------

void MainWindow::onDismissRequested(quint8 characterId, quint16 eventId)
{
    QByteArray payload;
    payload.append(static_cast<char>(characterId));
    payload.append(static_cast<char>(eventId & 0xFF));
    payload.append(static_cast<char>((eventId >> 8) & 0xFF));
    m_connectionManager.sendFrame(MSG_CLEAR_ATTENTION, CHARACTER_ID_DAEMON,
                                  payload);
}

// ---------------------------------------------------------------------------
// Character pane input
// ---------------------------------------------------------------------------

void MainWindow::onInputSubmitted(quint8 characterId, const QString &text)
{
    if (!m_connectionManager.hasControl())
    {
        return;
    }

    QByteArray payload;
    payload.append(static_cast<char>(0x00)); // source: Human
    const QByteArray utf8 = text.toUtf8();
    payload.append(static_cast<char>(utf8.size() & 0xFF));
    payload.append(static_cast<char>((utf8.size() >> 8) & 0xFF));
    payload.append(utf8);

    m_connectionManager.sendFrame(MSG_INPUT_ECHO, characterId, payload);
}

// ---------------------------------------------------------------------------
// UI state helpers
// ---------------------------------------------------------------------------

void MainWindow::updateConnectionUi(DaemonConnectionManager::State state)
{
    // Update quick-connect action label and state
    const QString lastProfile = ConnectionDialog::lastUsedProfile();
    if (lastProfile.isEmpty())
    {
        m_quickConnectAction->setText(QStringLiteral("Quick Connect"));
        m_quickConnectAction->setEnabled(false);
    }
    else
    {
        m_quickConnectAction->setText(
            QStringLiteral("Quick Connect: %1").arg(lastProfile));
        m_quickConnectAction->setEnabled(
            state == DaemonConnectionManager::State::Disconnected);
    }

    switch (state)
    {
        case DaemonConnectionManager::State::Disconnected:
            m_statusLabel->setText(QStringLiteral("Disconnected"));
            m_connectAction->setEnabled(true);
            m_disconnectAction->setEnabled(true);
            m_pathEditorAction->setEnabled(false);
            m_settingsAction->setEnabled(false);
            break;

        case DaemonConnectionManager::State::Connecting:
            m_statusLabel->setText(
                QStringLiteral("Connecting to %1:%2...")
                    .arg(m_connectionManager.host())
                    .arg(m_connectionManager.port()));
            m_connectAction->setEnabled(true);
            m_disconnectAction->setEnabled(true);
            m_pathEditorAction->setEnabled(false);
            m_settingsAction->setEnabled(false);
            break;

        case DaemonConnectionManager::State::Syncing:
            m_statusLabel->setText(
                QStringLiteral("Syncing with %1:%2...")
                    .arg(m_connectionManager.host())
                    .arg(m_connectionManager.port()));
            m_connectAction->setEnabled(true);
            m_disconnectAction->setEnabled(true);
            m_pathEditorAction->setEnabled(false);
            m_settingsAction->setEnabled(false);
            break;

        case DaemonConnectionManager::State::Connected:
        {
            QString controlLabel;
            if (m_connectionManager.hasControl())
            {
                controlLabel = QStringLiteral(" [control]");
            }
            else
            {
                controlLabel = QStringLiteral(" [observer]");
            }
            m_statusLabel->setText(
                QStringLiteral("Connected to %1:%2%3")
                    .arg(m_connectionManager.host())
                    .arg(m_connectionManager.port())
                    .arg(controlLabel));
            m_connectAction->setEnabled(true);
            m_disconnectAction->setEnabled(true);
            m_pathEditorAction->setEnabled(true);
            m_settingsAction->setEnabled(true);
            break;
        }
    }
}

void MainWindow::lockAllPanes(bool locked)
{
    for (CharacterPane *pane : m_panes)
    {
        pane->setInputLocked(locked);
    }
}

void MainWindow::applyObserverMode(bool observer)
{
    for (CharacterPane *pane : m_panes)
    {
        pane->setObserverMode(observer);
    }

    // Refresh status bar text to show control state
    updateConnectionUi(m_connectionManager.state());
}

// ---------------------------------------------------------------------------
// Pane management
// ---------------------------------------------------------------------------

CharacterPane *MainWindow::paneForCharacter(quint8 characterId) const
{
    return m_panes.value(characterId, nullptr);
}

CharacterPane *MainWindow::getOrCreatePane(quint8 characterId,
                                           const QString &characterName)
{
    CharacterPane *pane = paneForCharacter(characterId);
    if (pane != nullptr)
    {
        return pane;
    }

    pane = new CharacterPane(characterId, characterName,
                           TerminalWidget::DEFAULT_BACKBUFFER_LINES, this);
    connect(pane, &CharacterPane::inputSubmitted,
            this, &MainWindow::onInputSubmitted);

    m_panes.insert(characterId, pane);
    m_tabWidget->addTab(pane, characterName);

    // New panes inherit current lock state
    pane->setInputLocked(
        m_connectionManager.state() != DaemonConnectionManager::State::Connected);
    pane->setObserverMode(!m_connectionManager.hasControl());

    return pane;
}

void MainWindow::handleRoomIdentified(quint8 characterId,
                                      const QByteArray &payload)
{
    // Wire format: [room_id: 2][room_name_len: 1][room_name: N]
    if (payload.size() < 3)
    {
        return;
    }

    CharacterPane *pane = paneForCharacter(characterId);
    if (pane == nullptr)
    {
        return;
    }

    const int nameLen = static_cast<quint8>(payload[2]);
    if (payload.size() < 3 + nameLen)
    {
        return;
    }

    const QString roomName = QString::fromUtf8(payload.mid(3, nameLen));
    pane->setRoomName(roomName);
}

void MainWindow::handleEngineStatus(quint8 characterId,
                                    const QByteArray &payload)
{
    // Wire format: [mode: 1][combat_flag: 1][path_id: 2][current_step: 2][engine_state: 1]
    if (payload.size() < 7)
    {
        return;
    }

    CharacterPane *pane = paneForCharacter(characterId);
    if (pane == nullptr)
    {
        return;
    }

    const auto mode       = static_cast<EngineMode> (static_cast<quint8>(payload[0]));
    const auto combatFlag = static_cast<CombatFlag> (static_cast<quint8>(payload[1]));
    const auto state      = static_cast<EngineState>(static_cast<quint8>(payload[6]));

    pane->setEngineMode(mode);
    pane->setCombatFlag(combatFlag);

    // Script running = engine is active (not Off or Idle)
    const bool scriptRunning = (mode != EngineMode::Off)
                            && (state != EngineState::Idle);
    pane->setScriptRunning(scriptRunning);
}
