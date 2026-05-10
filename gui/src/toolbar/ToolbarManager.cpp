#include "toolbar/ToolbarManager.h"
#include "toolbar/IconFactory.h"
#include "window/MainWindow.h"

#include <QToolBar>
#include <QAction>

// ---------------------------------------------------------------------------
// Static
// ---------------------------------------------------------------------------

QStringList ToolbarManager::defaultButtonOrder()
{
    return QStringList()
        << QString::fromLatin1(ID_BBS_CONNECTION)
        << QString::fromLatin1(ID_GOTO_LOCATION)
        << QString::fromLatin1(ID_RUN_CIRCUIT)
        << QString::fromLatin1(ID_CEASE)
        << QStringLiteral("separator")
        << QString::fromLatin1(ID_AUTOMATION)
        << QString::fromLatin1(ID_AUTO_COMBAT)
        << QString::fromLatin1(ID_AUTO_REST)
        << QString::fromLatin1(ID_AUTO_BLESS)
        << QString::fromLatin1(ID_AUTO_GET_ITEMS)
        << QString::fromLatin1(ID_AUTO_GET_MONEY)
        << QString::fromLatin1(ID_AUTO_SNEAK)
        << QString::fromLatin1(ID_AUTO_HIDE)
        << QString::fromLatin1(ID_AUTO_SEARCH)
        << QStringLiteral("separator")
        << QString::fromLatin1(ID_SETTINGS);
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ToolbarManager::ToolbarManager(MainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    createActions();
    createToolbar();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QAction *ToolbarManager::action(const QString &id) const
{
    return m_actions.value(id, nullptr);
}

void ToolbarManager::applyPreConnectionState(QAction *connectAction,
                                                QAction *quickConnectAction,
                                                QAction *autoConnectAction)
{
    m_toolbar->clear();

    // Show File-menu equivalents with icons so the toolbar is useful
    // before the daemon is connected.
    if (connectAction != nullptr)
    {
        if (connectAction->icon().isNull())
        {
            connectAction->setIcon(IconFactory::daemonConnect());
        }
        m_toolbar->addAction(connectAction);
    }

    if (quickConnectAction != nullptr)
    {
        if (quickConnectAction->icon().isNull())
        {
            quickConnectAction->setIcon(IconFactory::daemonQuickConnect());
        }
        m_toolbar->addAction(quickConnectAction);
    }

    if (autoConnectAction != nullptr)
    {
        if (autoConnectAction->icon().isNull())
        {
            autoConnectAction->setIcon(IconFactory::daemonAutoConnect());
        }
        m_toolbar->addAction(autoConnectAction);
    }
}

void ToolbarManager::applyPostConnectionState(const QStringList &buttonOrder)
{
    m_toolbar->clear();

    // Always start with BBS connection toggle on the far left
    QAction *conn = m_actions.value(QString::fromLatin1(ID_BBS_CONNECTION));
    if (conn != nullptr)
    {
        m_toolbar->addAction(conn);
    }

    // Separator between the connect group and the rest
    m_toolbar->addSeparator();

    // Add remaining buttons per saved order.
    // Strip any leading separator -- the hardcoded one above already provides it.
    int startIndex = 0;
    while (startIndex < buttonOrder.size()
           && buttonOrder.at(startIndex) == QStringLiteral("separator"))
    {
        startIndex++;
    }

    for (int i = startIndex; i < buttonOrder.size(); ++i)
    {
        const QString &id = buttonOrder.at(i);
        if (id == QStringLiteral("separator"))
        {
            m_toolbar->addSeparator();
            continue;
        }

        // Skip BBS connect/disconnect — already added above
        if (id == QString::fromLatin1(ID_BBS_CONNECTION))
        {
            continue;
        }

        QAction *a = m_actions.value(id);
        if (a != nullptr)
        {
            m_toolbar->addAction(a);
        }
    }
}

void ToolbarManager::setBbsConnected(bool connected)
{
    QAction *a = m_actions.value(QString::fromLatin1(ID_BBS_CONNECTION));
    if (a != nullptr)
    {
        // Block signal so we don't emit bbsConnectionToggled during a
        // programmatic state update (e.g. daemon reporting connection status)
        a->blockSignals(true);
        a->setChecked(connected);
        a->blockSignals(false);
    }
}

void ToolbarManager::setAutomationChildrenEnabled(bool enabled)
{
    for (QAction *child : m_automationChildren)
    {
        child->setEnabled(enabled);
    }
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void ToolbarManager::onAutomationToggled(bool checked)
{
    setAutomationChildrenEnabled(checked);
    emit automationToggled(checked);
}

void ToolbarManager::onChildToggled(bool checked)
{
    QAction *sender = qobject_cast<QAction *>(QObject::sender());
    if (sender == nullptr)
    {
        return;
    }

    const QString id = m_actions.key(sender);
    if (!id.isEmpty())
    {
        emit automationChildToggled(id, checked);
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ToolbarManager::createActions()
{
    // --- BBS connection toggle ---
    {
        QAction *a = new QAction(IconFactory::bbsConnection(),
                                 QStringLiteral("BBS Connection"), this);
        a->setToolTip(QStringLiteral("Toggle character session connection to BBS server"));
        a->setCheckable(true);
        a->setChecked(false);
        a->setEnabled(false); // enabled once GUI is connected to daemon
        connect(a, &QAction::toggled, this, &ToolbarManager::bbsConnectionToggled);
        m_actions.insert(QString::fromLatin1(ID_BBS_CONNECTION), a);
    }

    // --- Path / navigation ---
    {
        QAction *a = new QAction(IconFactory::gotoLocation(),
                                 QStringLiteral("Go To Location"), this);
        a->setToolTip(QStringLiteral("Navigate to a specific location"));
        a->setEnabled(false);
        connect(a, &QAction::triggered, this, &ToolbarManager::gotoLocationRequested);
        m_actions.insert(QString::fromLatin1(ID_GOTO_LOCATION), a);
    }
    {
        QAction *a = new QAction(IconFactory::runCircuit(),
                                 QStringLiteral("Run Circuit"), this);
        a->setToolTip(QStringLiteral("Start a farming circuit"));
        a->setEnabled(false);
        connect(a, &QAction::triggered, this, &ToolbarManager::runCircuitRequested);
        m_actions.insert(QString::fromLatin1(ID_RUN_CIRCUIT), a);
    }
    {
        QAction *a = new QAction(IconFactory::cease(),
                                 QStringLiteral("Cease"), this);
        a->setToolTip(QStringLiteral("Stop current path or circuit"));
        a->setEnabled(false);
        connect(a, &QAction::triggered, this, &ToolbarManager::ceaseRequested);
        m_actions.insert(QString::fromLatin1(ID_CEASE), a);
    }

    // --- Automation master ---
    {
        QAction *a = new QAction(IconFactory::automationToggle(),
                                 QStringLiteral("Automation"), this);
        a->setToolTip(QStringLiteral("Enable or disable all automation"));
        a->setCheckable(true);
        a->setChecked(false);
        a->setEnabled(false);
        connect(a, &QAction::toggled, this, &ToolbarManager::onAutomationToggled);
        m_actions.insert(QString::fromLatin1(ID_AUTOMATION), a);
    }

    // --- Automation children ---
    // Helper lambda to create a checkable child automation action
    auto makeChildAction = [this](const char *id, const QIcon &icon,
                                  const QString &label, const QString &tip)
    {
        QAction *a = new QAction(icon, label, this);
        a->setToolTip(tip);
        a->setCheckable(true);
        a->setChecked(false);
        a->setEnabled(false); // controlled by master toggle
        connect(a, &QAction::toggled, this, &ToolbarManager::onChildToggled);
        m_actions.insert(QString::fromLatin1(id), a);
        m_automationChildren.append(a);
    };

    makeChildAction(ID_AUTO_COMBAT,
                    IconFactory::autoCombat(),
                    QStringLiteral("Auto Combat"),
                    QStringLiteral("Automatically engage monsters per alignment rules"));

    makeChildAction(ID_AUTO_REST,
                    IconFactory::autoRest(),
                    QStringLiteral("Auto Rest"),
                    QStringLiteral("Automatically rest and heal when HP/mana is low"));

    makeChildAction(ID_AUTO_BLESS,
                    IconFactory::autoBless(),
                    QStringLiteral("Auto Bless"),
                    QStringLiteral("Automatically request blessings from party members"));

    makeChildAction(ID_AUTO_GET_ITEMS,
                    IconFactory::autoGetItems(),
                    QStringLiteral("Auto Get Items"),
                    QStringLiteral("Automatically collect sought items from rooms"));

    makeChildAction(ID_AUTO_GET_MONEY,
                    IconFactory::autoGetMoney(),
                    QStringLiteral("Auto Get Money"),
                    QStringLiteral("Automatically collect currency from rooms"));

    makeChildAction(ID_AUTO_SNEAK,
                    IconFactory::autoSneak(),
                    QStringLiteral("Auto Sneak"),
                    QStringLiteral("Automatically sneak between rooms"));

    makeChildAction(ID_AUTO_HIDE,
                    IconFactory::autoHide(),
                    QStringLiteral("Auto Hide"),
                    QStringLiteral("Automatically hide after movement"));

    makeChildAction(ID_AUTO_SEARCH,
                    IconFactory::autoSearch(),
                    QStringLiteral("Auto Search"),
                    QStringLiteral("Automatically search rooms at search points"));

    // --- Settings ---
    {
        QAction *a = new QAction(IconFactory::settings(),
                                 QStringLiteral("Settings"), this);
        a->setToolTip(QStringLiteral("Open settings dialog"));
        a->setEnabled(false);
        connect(a, &QAction::triggered, this, &ToolbarManager::settingsRequested);
        m_actions.insert(QString::fromLatin1(ID_SETTINGS), a);
    }
}

void ToolbarManager::createToolbar()
{
    m_toolbar = new QToolBar(QStringLiteral("Main Toolbar"), m_mainWindow);
    m_toolbar->setObjectName(QStringLiteral("MainToolbar"));
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(24, 24));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    // MainWindow calls applyPreConnectionState() with the File menu actions
    // immediately after construction - nothing to do here.
}
