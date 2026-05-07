#pragma once

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QString>
#include "protocol/Protocol.h"

// ---------------------------------------------------------------------------
// StatusLine
//
// Horizontal bar displayed below the terminal and above the input widget.
// Shows per-character status using QLabel segments separated by thin
// dividers. All labels update via setters — no polling.
//
// Layout (left to right):
//   [character name] | [BBS/server name] | [status] | [engine mode]
//   [combat flag] | [room name] | [script indicator]
//
// Colour coding for connection status:
//   Disconnected  — grey
//   Connecting    — yellow
//   Connected     — green
//   Reconnecting  — orange
//   Suspended     — blue
//   Error         — red
//
// Engine mode shown as text: Off / Semi / Auto
// Combat flag shown as:  ⚔ (sword, Attack mode) or blank (Passive)
// Script running shown as: ● (filled circle, bright green) or blank
// ---------------------------------------------------------------------------

class StatusLine : public QWidget
{
    Q_OBJECT

public:
    explicit StatusLine(const QString &characterName,
                        const QString &serverName,
                        QWidget *parent = nullptr);

    // Setters — each triggers a label update only for the changed segment
    void setCharacterStatus(CharacterStatus status);
    void setEngineMode(EngineMode mode);
    void setCombatFlag(CombatFlag flag);
    void setRoomName(const QString &roomName);
    void setScriptRunning(bool running);
    void setServerName(const QString &serverName);

private:
    static QString statusText(CharacterStatus status);
    static QColor  statusColour(CharacterStatus status);
    static QString engineModeText(EngineMode mode);

    void applyStatusColour(CharacterStatus status);

    QLabel *m_charNameLabel   = nullptr;
    QLabel *m_serverLabel     = nullptr;
    QLabel *m_statusLabel     = nullptr;
    QLabel *m_engineLabel     = nullptr;
    QLabel *m_combatLabel     = nullptr;
    QLabel *m_roomLabel       = nullptr;
    QLabel *m_scriptLabel     = nullptr;

    CharacterStatus m_status      = CharacterStatus::Disconnected;
    EngineMode      m_engineMode  = EngineMode::Off;
    CombatFlag      m_combatFlag  = CombatFlag::Passive;
    bool            m_scriptRunning = false;
};
