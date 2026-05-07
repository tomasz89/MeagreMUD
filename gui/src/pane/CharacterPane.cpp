#include "pane/CharacterPane.h"

#include <QVBoxLayout>

CharacterPane::CharacterPane(quint8 characterId, const QString &characterName,
                             int backbufferCapacity, QWidget *parent)
    : QWidget(parent)
    , m_characterId(characterId)
    , m_characterName(characterName)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_terminal = new TerminalWidget(backbufferCapacity, this);
    layout->addWidget(m_terminal, 1);

    m_statusLine = new StatusLine(characterName,
                                  QStringLiteral("(no server)"), this);
    layout->addWidget(m_statusLine);

    m_inputWidget = new InputWidget(this);
    layout->addWidget(m_inputWidget);

    connect(m_inputWidget, &InputWidget::textSubmitted,
            this, &CharacterPane::onTextSubmitted);
}

// ---------------------------------------------------------------------------
// Status / terminal forwarding
// ---------------------------------------------------------------------------

void CharacterPane::setCharacterStatus(CharacterStatus status)
{
    m_status = status;
    m_statusLine->setCharacterStatus(status);
}

void CharacterPane::appendRun(const StyledRun &run)
{
    m_terminal->appendRun(run);
}

void CharacterPane::clearScreen()
{
    m_terminal->clearScreen();
}

void CharacterPane::setEngineMode(EngineMode mode)
{
    m_statusLine->setEngineMode(mode);
}

void CharacterPane::setCombatFlag(CombatFlag flag)
{
    m_statusLine->setCombatFlag(flag);
}

void CharacterPane::setRoomName(const QString &roomName)
{
    m_statusLine->setRoomName(roomName);
}

void CharacterPane::setScriptRunning(bool running)
{
    m_statusLine->setScriptRunning(running);
}

void CharacterPane::setServerName(const QString &serverName)
{
    m_statusLine->setServerName(serverName);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void CharacterPane::setInputLocked(bool locked)
{
    m_inputLocked = locked;
    m_inputWidget->setLocked(locked);
}

void CharacterPane::setObserverMode(bool observer)
{
    m_observerMode = observer;
    setInputLocked(observer);
}

void CharacterPane::addMacro(int key, Qt::KeyboardModifiers modifiers,
                             const QString &macroText)
{
    m_inputWidget->addMacro(key, modifiers, macroText);
}

void CharacterPane::clearMacros()
{
    m_inputWidget->clearMacros();
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void CharacterPane::onTextSubmitted(const QString &text)
{
    emit inputSubmitted(m_characterId, text);
}
