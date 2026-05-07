#include "window/MainWindow.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QDebug>

#include "protocol/Protocol.h"
#include "types/MudTypes.h"

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
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void MainWindow::setupMenuBar()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));

    m_connectAction = fileMenu->addAction(QStringLiteral("&Connect to Daemon…"));
    connect(m_connectAction, &QAction::triggered,
            this, &MainWindow::onActionConnect);

    m_disconnectAction = fileMenu->addAction(QStringLiteral("&Disconnect"));
    connect(m_disconnectAction, &QAction::triggered,
            this, &MainWindow::onActionDisconnect);

    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction(QStringLiteral("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered,
            qApp, &QApplication::quit);

    // Tools menu
    QMenu *toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));

    QAction *pathEditorAction = toolsMenu->addAction(
        QStringLiteral("&Path Editor…"));
    connect(pathEditorAction, &QAction::triggered,
            this, &MainWindow::onActionPathEditor);

    toolsMenu->addSeparator();

    QAction *settingsAction = toolsMenu->addAction(
        QStringLiteral("&Settings…"));
    settingsAction->setShortcut(QKeySequence::Preferences);
    connect(settingsAction, &QAction::triggered,
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

    // Tiled overview — hidden until panes are added to it
    m_tiledArea = new TiledArea(central);
    layout->addWidget(m_tiledArea);

    // Attention panel drawer — sits above tab bar
    m_attentionPanel = new AttentionPanel(central);
    layout->addWidget(m_attentionPanel);
    connect(m_attentionPanel, &AttentionPanel::dismissRequested,
            this, &MainWindow::onDismissRequested);

    // Tab widget — one tab per docked character
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
    if (m_connectionManager.state() != DaemonConnectionManager::State::Disconnected)
    {
        return;
    }

    const QString host = SettingsDialog::savedHost();
    const quint16 port = SettingsDialog::savedPort();

    if (host.isEmpty())
    {
        // No host configured yet -- open settings at the daemon connection tab
        SettingsDialog dlg(this);
        connect(&dlg, &SettingsDialog::daemonConnectionChanged,
                this, &MainWindow::onDaemonConnectionSettingsChanged);
        dlg.setCurrentTab(SettingsDialog::Tab::DaemonConnection);
        if (dlg.exec() != QDialog::Accepted)
        {
            return;
        }
    }

    m_connectionManager.connectToDaemon(
        SettingsDialog::savedHost(),
        SettingsDialog::savedPort());
}
void MainWindow::onActionDisconnect()
{
    m_connectionManager.disconnectFromDaemon();
}

void MainWindow::onActionPathEditor()
{
    // PathEditor window — implemented separately
    QMessageBox::information(this, QStringLiteral("Path Editor"),
                             QStringLiteral("Path Editor not yet implemented."));
}

void MainWindow::onActionSettings()
{
    SettingsDialog dlg(this);
    connect(&dlg, &SettingsDialog::daemonConnectionChanged,
            this, &MainWindow::onDaemonConnectionSettingsChanged);
    dlg.exec();
}
void MainWindow::onDaemonConnectionSettingsChanged()
{
    // Settings were saved. If we are currently disconnected the new host/port
    // will be picked up automatically on the next connect attempt.
    // If already connected, inform the user the change takes effect on
    // next connection.
    if (m_connectionManager.state() != DaemonConnectionManager::State::Disconnected)
    {
        statusBar()->showMessage(
            QStringLiteral("[MeagreMUD] Daemon connection settings updated"
                           " — takes effect on next connection."),
            5000);
    }
}

void MainWindow::onActionAbout()
{
    QMessageBox::about(this,
        QStringLiteral("About MeagreMUD"),
        QStringLiteral(
            "<b>MeagreMUD</b> v0.1.0<br>"
            "Open source MegaMUD clone.<br>"
            "GPL-3.0 — targeting GreaterMUD / MajorMUD servers."));
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
            break;

        case DaemonConnectionManager::State::Connecting:
            lockAllPanes(true);
            break;

        case DaemonConnectionManager::State::Syncing:
            // Lock inputs until ResyncAck — panes dimmed, only Disconnect available
            lockAllPanes(true);

            if (m_connectionManager.resyncCount() > 0)
            {
                const QString msg = QStringLiteral("[MeagreMUD] Protocol resync #%1")
                    .arg(m_connectionManager.resyncCount());
                qDebug() << msg;
                // TODO: display in active character pane as a server message
            }
            break;

        case DaemonConnectionManager::State::Connected:
            // Unlock inputs — observer mode governed separately by hasControl
            lockAllPanes(false);
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
    // Nothing to do at this level — panes are already created.
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
    // Rendered with [MeagreMUD] prefix in blue — TerminalWidget handles this
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
    switch (state)
    {
        case DaemonConnectionManager::State::Disconnected:
            m_statusLabel->setText(QStringLiteral("Disconnected"));
            m_connectAction->setEnabled(true);
            m_disconnectAction->setEnabled(false);
            break;

        case DaemonConnectionManager::State::Connecting:
            m_statusLabel->setText(
                QStringLiteral("Connecting to %1:%2…")
                    .arg(m_connectionManager.host())
                    .arg(m_connectionManager.port()));
            m_connectAction->setEnabled(false);
            m_disconnectAction->setEnabled(true);
            break;

        case DaemonConnectionManager::State::Syncing:
            m_statusLabel->setText(
                QStringLiteral("Syncing with %1:%2…")
                    .arg(m_connectionManager.host())
                    .arg(m_connectionManager.port()));
            m_connectAction->setEnabled(false);
            m_disconnectAction->setEnabled(true);
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
            m_connectAction->setEnabled(false);
            m_disconnectAction->setEnabled(true);
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
