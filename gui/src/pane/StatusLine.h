#pragma once

/**
 * @file StatusLine.h
 * @brief Per-character status bar displayed between the terminal and input widgets.
 */

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QString>
#include "protocol/Protocol.h"

/**
 * @brief Horizontal status bar showing per-character runtime state.
 *
 * Uses a row of QLabel segments separated by vertical QFrame dividers.
 * All segments update independently via individual setter methods —
 * no polling, no full-bar repaints.
 *
 * ## Layout (left to right)
 * | Segment | Content |
 * |---------|---------|
 * | Character name | Bold; fixed at construction |
 * | Server name | Updatable via setServerName() |
 * | Connection status | Colour-coded text |
 * | Engine mode | Off / Semi / Auto |
 * | Combat flag | ⚔ (Attack) or blank (Passive) |
 * | Room name | Stretches to fill available width |
 * | Script indicator | ● (green) when script active |
 *
 * ## Status colours
 * | Status | Colour |
 * |--------|--------|
 * | Disconnected | Grey |
 * | Connecting | Yellow |
 * | Connected | Green |
 * | Reconnecting | Orange |
 * | Suspended | Blue |
 * | Error | Red |
 *
 * @see CharacterPane, CharacterStatus, EngineMode, CombatFlag
 */
class StatusLine : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Construct the status line.
     * @param characterName  Character name shown in the leftmost segment (fixed).
     * @param serverName     Initial server name shown in the second segment.
     * @param parent         Qt parent widget.
     */
    explicit StatusLine(const QString &characterName,
                        const QString &serverName,
                        QWidget *parent = nullptr);

    /**
     * @brief Update the connection status segment.
     * @param status  New character connection status.
     */
    void setCharacterStatus(CharacterStatus status);

    /**
     * @brief Update the engine mode segment.
     * @param mode  New engine operating mode.
     */
    void setEngineMode(EngineMode mode);

    /**
     * @brief Update the combat flag segment.
     *
     * Shows ⚔ when Attack, blank when Passive.
     *
     * @param flag  New combat flag value.
     */
    void setCombatFlag(CombatFlag flag);

    /**
     * @brief Update the room name segment.
     * @param roomName  Current room name. Shows "(unknown room)" if empty.
     */
    void setRoomName(const QString &roomName);

    /**
     * @brief Update the script running indicator.
     * @param running  @c true to show the green ● indicator; @c false to hide it.
     */
    void setScriptRunning(bool running);

    /**
     * @brief Update the server name segment.
     * @param serverName  New server name.
     */
    void setServerName(const QString &serverName);

private:
    static QString statusText(CharacterStatus status);
    static QColor  statusColour(CharacterStatus status);
    static QString engineModeText(EngineMode mode);
    void           applyStatusColour(CharacterStatus status);

    QLabel *m_charNameLabel  = nullptr; ///< Bold character name; never changes.
    QLabel *m_serverLabel    = nullptr; ///< Server/BBS name.
    QLabel *m_statusLabel    = nullptr; ///< Connection status text (colour-coded).
    QLabel *m_engineLabel    = nullptr; ///< Engine mode text.
    QLabel *m_combatLabel    = nullptr; ///< Combat flag icon (⚔ or blank).
    QLabel *m_roomLabel      = nullptr; ///< Current room name (stretches).
    QLabel *m_scriptLabel    = nullptr; ///< Script running indicator (● or blank).

    CharacterStatus m_status       = CharacterStatus::Disconnected;
    EngineMode      m_engineMode   = EngineMode::Off;
    CombatFlag      m_combatFlag   = CombatFlag::Passive;
    bool            m_scriptRunning = false;
};
