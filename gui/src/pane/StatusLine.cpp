#include "pane/StatusLine.h"

#include <QFrame>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

StatusLine::StatusLine(const QString &characterName,
                       const QString &serverName,
                       QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    // Helper lambda to add a vertical divider
    auto addDivider = [&]()
    {
        QFrame *line = new QFrame(this);
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);
        layout->addWidget(line);
    };

    // Character name  -  fixed, bold
    m_charNameLabel = new QLabel(characterName, this);
    QFont boldFont = m_charNameLabel->font();
    boldFont.setBold(true);
    m_charNameLabel->setFont(boldFont);
    layout->addWidget(m_charNameLabel);

    addDivider();

    // Server name
    m_serverLabel = new QLabel(serverName, this);
    layout->addWidget(m_serverLabel);

    addDivider();

    // Connection status
    m_statusLabel = new QLabel(this);
    layout->addWidget(m_statusLabel);

    addDivider();

    // Engine mode
    m_engineLabel = new QLabel(this);
    layout->addWidget(m_engineLabel);

    addDivider();

    // Combat flag  -  sword icon when Attack, blank when Passive
    m_combatLabel = new QLabel(this);
    m_combatLabel->setFixedWidth(20);
    m_combatLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_combatLabel);

    addDivider();

    // Room name  -  stretches to fill remaining space
    m_roomLabel = new QLabel(QStringLiteral("(unknown room)"), this);
    m_roomLabel->setTextFormat(Qt::PlainText);
    layout->addWidget(m_roomLabel, 1);

    addDivider();

    // Script running indicator
    m_scriptLabel = new QLabel(this);
    m_scriptLabel->setFixedWidth(16);
    m_scriptLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_scriptLabel);

    // Apply initial state
    setCharacterStatus(CharacterStatus::Disconnected);
    setEngineMode(EngineMode::Off);
    setCombatFlag(CombatFlag::Passive);
    setScriptRunning(false);

    // Constrain height
    setFixedHeight(sizeHint().height());
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

// ---------------------------------------------------------------------------
// Setters
// ---------------------------------------------------------------------------

void StatusLine::setCharacterStatus(CharacterStatus status)
{
    m_status = status;
    m_statusLabel->setText(statusText(status));
    applyStatusColour(status);
}

void StatusLine::setEngineMode(EngineMode mode)
{
    m_engineMode = mode;
    m_engineLabel->setText(engineModeText(mode));
}

void StatusLine::setCombatFlag(CombatFlag flag)
{
    m_combatFlag = flag;
    if (flag == CombatFlag::Attack)
    {
        m_combatLabel->setText(QStringLiteral("\u2694")); // [sword] crossed swords
    }
    else
    {
        m_combatLabel->setText(QString());
    }
}

void StatusLine::setRoomName(const QString &roomName)
{
    if (roomName.isEmpty())
    {
        m_roomLabel->setText(QStringLiteral("(unknown room)"));
    }
    else
    {
        m_roomLabel->setText(roomName);
    }
}

void StatusLine::setScriptRunning(bool running)
{
    m_scriptRunning = running;
    if (running)
    {
        m_scriptLabel->setText(QStringLiteral("\u25CF")); // * filled circle
        QPalette p = m_scriptLabel->palette();
        p.setColor(QPalette::WindowText, QColor(0, 220, 0));
        m_scriptLabel->setPalette(p);
    }
    else
    {
        m_scriptLabel->setText(QString());
    }
}

void StatusLine::setServerName(const QString &serverName)
{
    m_serverLabel->setText(serverName);
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

QString StatusLine::statusText(CharacterStatus status)
{
    switch (status)
    {
        case CharacterStatus::Disconnected:  return QStringLiteral("Disconnected");
        case CharacterStatus::Connecting:    return QStringLiteral("Connecting...");
        case CharacterStatus::Connected:     return QStringLiteral("Connected");
        case CharacterStatus::Reconnecting:  return QStringLiteral("Reconnecting...");
        case CharacterStatus::Suspended:     return QStringLiteral("Suspended");
        case CharacterStatus::Error:         return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

QColor StatusLine::statusColour(CharacterStatus status)
{
    switch (status)
    {
        case CharacterStatus::Disconnected:  return QColor(140, 140, 140); // grey
        case CharacterStatus::Connecting:    return QColor(220, 180,   0); // yellow
        case CharacterStatus::Connected:     return QColor(  0, 200,   0); // green
        case CharacterStatus::Reconnecting:  return QColor(220, 120,   0); // orange
        case CharacterStatus::Suspended:     return QColor( 80, 140, 220); // blue
        case CharacterStatus::Error:         return QColor(220,  40,  40); // red
    }
    return QColor(140, 140, 140);
}

QString StatusLine::engineModeText(EngineMode mode)
{
    switch (mode)
    {
        case EngineMode::Off:      return QStringLiteral("Off");
        case EngineMode::SemiAuto: return QStringLiteral("Semi");
        case EngineMode::Auto:     return QStringLiteral("Auto");
    }
    return QStringLiteral("Off");
}

void StatusLine::applyStatusColour(CharacterStatus status)
{
    const QColor colour = statusColour(status);
    QPalette p = m_statusLabel->palette();
    p.setColor(QPalette::WindowText, colour);
    m_statusLabel->setPalette(p);
}
