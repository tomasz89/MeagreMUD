#pragma once

#include <QWidget>
#include <QString>
#include "protocol/Protocol.h"
#include "terminal/TerminalWidget.h"
#include "pane/StatusLine.h"
#include "input/InputWidget.h"

// ---------------------------------------------------------------------------
// CharacterPane
//
// The per-character UI unit: TerminalWidget + StatusLine + InputWidget.
// The same instance is reparented when docking/undocking between the
// QTabWidget and TiledArea.
//
// Observer mode: input is locked; terminal and status display are unaffected.
// ---------------------------------------------------------------------------

class CharacterPane : public QWidget
{
    Q_OBJECT

public:
    explicit CharacterPane(quint8 characterId, const QString &characterName,
                           int backbufferCapacity = TerminalWidget::DEFAULT_BACKBUFFER_LINES,
                           QWidget *parent = nullptr);

    quint8  characterId()   const { return m_characterId; }
    QString characterName() const { return m_characterName; }

    CharacterStatus status() const { return m_status; }
    void setCharacterStatus(CharacterStatus status);

    // Terminal forwarding
    void appendRun(const StyledRun &run);
    void clearScreen();

    // Status line updates
    void setEngineMode(EngineMode mode);
    void setCombatFlag(CombatFlag flag);
    void setRoomName(const QString &roomName);
    void setScriptRunning(bool running);
    void setServerName(const QString &serverName);

    // Input lock — called on resync and observer mode changes
    void setInputLocked(bool locked);
    bool isInputLocked() const { return m_inputLocked; }

    // Observer mode — input locked, no visual change to terminal or status
    void setObserverMode(bool observer);

    // Macro management — forwarded to InputWidget
    void addMacro(int key, Qt::KeyboardModifiers modifiers,
                  const QString &macroText);
    void clearMacros();

    TerminalWidget *terminalWidget() const { return m_terminal; }
    StatusLine     *statusLine()     const { return m_statusLine; }
    InputWidget    *inputWidget()    const { return m_inputWidget; }

signals:
    // Emitted when the user submits text. Text has \r\n appended.
    void inputSubmitted(quint8 characterId, const QString &text);

private slots:
    void onTextSubmitted(const QString &text);

private:
    quint8          m_characterId;
    QString         m_characterName;
    CharacterStatus m_status        = CharacterStatus::Disconnected;
    bool            m_observerMode  = false;
    bool            m_inputLocked   = false;

    TerminalWidget *m_terminal   = nullptr;
    StatusLine     *m_statusLine = nullptr;
    InputWidget    *m_inputWidget = nullptr;
};
