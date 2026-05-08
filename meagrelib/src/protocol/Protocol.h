#pragma once

#include <QtGlobal>

/**
 * @file Protocol.h
 * @brief Wire protocol constants, message type codes, and sub-type enumerations
 *        for the MeagreMUD daemon ↔ GUI binary protocol.
 *
 * Frame header layout (8 bytes):
 * @code
 * [version: 1][msg_type: 1][character_id: 1][flags: 1][sequence: 2][payload_len: 2]
 * @endcode
 *
 * - @c character_id 0x00 is reserved for daemon-level (non-character) messages.
 * - @c flags is reserved; always set to 0.
 * - @c sequence is a monotonic counter; rollover is detectable.
 * - Maximum 16 characters per daemon (character_id 1–16).
 */

// ---------------------------------------------------------------------------
// Frame header constants
// ---------------------------------------------------------------------------

/// Size of the frame header in bytes.
static constexpr int    FRAME_HEADER_SIZE    = 8;

/// Current protocol version sent in every frame header.
static constexpr quint8 PROTOCOL_VERSION     = 1;

/// character_id value used for daemon-level (non-character) messages.
static constexpr quint8 CHARACTER_ID_DAEMON  = 0x00;

// ---------------------------------------------------------------------------
// Message type codes
// ---------------------------------------------------------------------------

/// @name Handshake / Session (0x00–0x0F)
/// @{
static constexpr quint8 MSG_CLIENT_HELLO     = 0x00; ///< GUI → Daemon: open handshake.
static constexpr quint8 MSG_SERVER_HELLO     = 0x01; ///< Daemon → GUI: handshake accepted.
static constexpr quint8 MSG_SERVER_REJECT    = 0x02; ///< Daemon → GUI: handshake rejected.
static constexpr quint8 MSG_PING             = 0x03; ///< Either direction: keepalive ping.
static constexpr quint8 MSG_PONG             = 0x04; ///< Either direction: keepalive reply.
static constexpr quint8 MSG_GOODBYE          = 0x05; ///< Either direction: clean disconnect.
static constexpr quint8 MSG_RESYNC_REQUEST   = 0x06; ///< GUI → Daemon: request full state dump.
static constexpr quint8 MSG_RESYNC_ACK       = 0x07; ///< Daemon → GUI: resync starting.
/// @}

/// @name Character State / Dump (0x10–0x1F)
/// @{
static constexpr quint8 MSG_CHARACTER_INFO      = 0x10; ///< Daemon → GUI: character metadata.
static constexpr quint8 MSG_BACKBUFFER_BEGIN     = 0x11; ///< Daemon → GUI: start of backbuffer dump.
static constexpr quint8 MSG_BACKBUFFER_LINE      = 0x12; ///< Daemon → GUI: one historical line.
static constexpr quint8 MSG_BACKBUFFER_END       = 0x13; ///< Daemon → GUI: end of backbuffer dump.
static constexpr quint8 MSG_CHARACTER_INFO_END   = 0x14; ///< Daemon → GUI: end of character info block.
/// @}

/// @name Incremental Stream (0x20–0x2F)
/// @{
static constexpr quint8 MSG_STYLED_RUN       = 0x20; ///< Daemon → GUI: one parsed StyledRun.
static constexpr quint8 MSG_STATUS_CHANGE    = 0x21; ///< Daemon → GUI: character connection status changed.
static constexpr quint8 MSG_INPUT_ECHO       = 0x22; ///< GUI → Daemon: user or script input text.
static constexpr quint8 MSG_SERVER_MESSAGE   = 0x23; ///< Daemon → GUI: [MeagreMUD] prefixed system message.
/// @}

/// @name Control (0x30–0x3F)
/// @{
static constexpr quint8 MSG_CONTROL_STATUS   = 0x30; ///< Daemon → GUI: current controller status.
static constexpr quint8 MSG_REQUEST_CONTROL  = 0x31; ///< GUI → Daemon: request controller role.
static constexpr quint8 MSG_CONTROL_GRANTED  = 0x32; ///< Daemon → GUI: this GUI is now controller.
static constexpr quint8 MSG_CONTROL_REVOKED  = 0x33; ///< Daemon → GUI: this GUI is now observer.
static constexpr quint8 MSG_CONTROL_DENIED   = 0x34; ///< Daemon → GUI: control request denied.
/// @}

/// @name Scripting / Automation (0x40–0x4F)
/// @{
static constexpr quint8 MSG_SET_ENGINE_MODE      = 0x40; ///< GUI → Daemon: set engine mode for character.
static constexpr quint8 MSG_SET_ACTIVE_PATH      = 0x41; ///< GUI → Daemon: set active path for character.
static constexpr quint8 MSG_PREFLIGHT_REQUEST    = 0x42; ///< GUI → Daemon: run preflight check on path.
static constexpr quint8 MSG_CLEAR_ATTENTION      = 0x43; ///< GUI → Daemon: dismiss an attention event.
static constexpr quint8 MSG_STEP_OVERRIDE        = 0x44; ///< GUI → Daemon: override current path step.
static constexpr quint8 MSG_ABORT_PATH           = 0x45; ///< GUI → Daemon: abort active path.
static constexpr quint8 MSG_SET_COMBAT_FLAG      = 0x46; ///< GUI → Daemon: set combat flag for character.
static constexpr quint8 MSG_STATS_RESET          = 0x47; ///< GUI → Daemon: reset session statistics odometer.
static constexpr quint8 MSG_ENGINE_STATUS        = 0x48; ///< Daemon → GUI: engine state update.
static constexpr quint8 MSG_PREFLIGHT_RESULT     = 0x49; ///< Daemon → GUI: preflight check results.
static constexpr quint8 MSG_ATTENTION_EVENT      = 0x4A; ///< Daemon → GUI: attention event raised.
static constexpr quint8 MSG_ATTENTION_CLEARED    = 0x4B; ///< Daemon → GUI: attention event dismissed.
static constexpr quint8 MSG_ROOM_IDENTIFIED      = 0x4C; ///< Daemon → GUI: room identified from output.
static constexpr quint8 MSG_STATS_UPDATE         = 0x4D; ///< Daemon → GUI: session statistics payload.
/// @}

/// @name Recording (0x50–0x5F)
/// @{
static constexpr quint8 MSG_RECORDING_START      = 0x50; ///< GUI → Daemon: begin recording for character.
static constexpr quint8 MSG_RECORDING_STEP       = 0x51; ///< Daemon → GUI: one recorded command step.
static constexpr quint8 MSG_RECORDING_STOP       = 0x52; ///< GUI → Daemon: stop recording.
/// @}

/// @name Errors / Diagnostics (0xF0–0xFF)
/// @{
static constexpr quint8 MSG_PROTOCOL_ERROR       = 0xF0; ///< Either direction: protocol error notification.
static constexpr quint8 MSG_DIAGNOSTIC_INFO      = 0xF1; ///< Daemon → GUI: diagnostic text.
static constexpr quint8 MSG_FATAL_ERROR          = 0xF2; ///< Daemon → GUI: unrecoverable error.
static constexpr quint8 MSG_DEBUG_DUMP_REQUEST   = 0xF3; ///< GUI → Daemon: request debug dump.
static constexpr quint8 MSG_DEBUG_DUMP_BEGIN     = 0xF4; ///< Daemon → GUI: debug dump starting.
static constexpr quint8 MSG_DEBUG_DUMP_LINE      = 0xF5; ///< Daemon → GUI: one line of debug dump.
static constexpr quint8 MSG_DEBUG_DUMP_END       = 0xF6; ///< Daemon → GUI: debug dump complete.
/// @}

// ---------------------------------------------------------------------------
// Sub-type enumerations
// ---------------------------------------------------------------------------

/**
 * @brief Connection status of a character's MUD session.
 * @see MSG_STATUS_CHANGE, MSG_CHARACTER_INFO
 */
enum class CharacterStatus : quint8 {
    Disconnected = 0x00, ///< Not connected to MUD.
    Connecting   = 0x01, ///< TCP connect in progress.
    Connected    = 0x02, ///< Connected and active.
    Reconnecting = 0x03, ///< Lost connection; reconnect pending.
    Suspended    = 0x04, ///< Session suspended (e.g. auto-suspend threshold).
    Error        = 0x05, ///< Unrecoverable session error.
};

/**
 * @brief Automation engine operating mode for a character.
 * @see MSG_SET_ENGINE_MODE, MSG_ENGINE_STATUS
 */
enum class EngineMode : quint8 {
    Off      = 0x00, ///< All automation disabled; full manual control.
    SemiAuto = 0x01, ///< Reactive triggers active; path engine inactive.
    Auto     = 0x02, ///< Path engine and reactive triggers both active.
};

/**
 * @brief Combat behaviour flag for the path engine.
 * @see MSG_SET_COMBAT_FLAG, MSG_ENGINE_STATUS
 */
enum class CombatFlag : quint8 {
    Passive = 0x00, ///< Never initiate or retaliate; keep moving.
    Attack  = 0x01, ///< Engage hostiles immediately; engage neutrals if clearing.
};

/**
 * @brief Detailed state of the path engine for a character.
 * @see MSG_ENGINE_STATUS
 */
enum class EngineState : quint8 {
    Idle            = 0x00, ///< No path active.
    Running         = 0x01, ///< Path executing normally.
    Paused          = 0x02, ///< Paused (combat or explicit wait).
    Retrying        = 0x03, ///< Room fingerprint mismatch; retrying.
    AwaitingHuman   = 0x04, ///< Attention event raised; waiting for user action.
    PreflightFailed = 0x05, ///< Preflight check failed; cannot start.
    Fleeing         = 0x06, ///< Executing flee sequence.
    Resting         = 0x07, ///< Resting to recover HP.
    Meditating      = 0x08, ///< Meditating to recover mana.
    KnockedDown     = 0x09, ///< Character is knocked down; movement suppressed.
    Blind           = 0x0A, ///< Character is blind; movement handling per blindness_mode.
};

/**
 * @brief Severity level of a daemon server message.
 * @see MSG_SERVER_MESSAGE
 */
enum class MessageSeverity : quint8 {
    Info    = 0x00, ///< Informational; shown with [MeagreMUD] prefix in blue.
    Warning = 0x01, ///< Warning condition.
    Error   = 0x02, ///< Error condition.
};

/**
 * @brief Source of an input echo message.
 * @see MSG_INPUT_ECHO
 */
enum class InputSource : quint8 {
    Human  = 0x00, ///< Text typed by the user.
    Script = 0x01, ///< Text sent by the script/path engine.
};

/**
 * @brief Reason a control request was denied.
 * @see MSG_CONTROL_DENIED
 */
enum class ControlDeniedReason : quint8 {
    AlreadyController = 0x00, ///< This GUI is already the controller.
    NotFound          = 0x01, ///< Character not found.
};

/**
 * @brief Protocol-level error codes.
 * @see MSG_PROTOCOL_ERROR
 */
enum class ProtocolErrorCode : quint8 {
    UnknownMessageType = 0x00, ///< Received an unrecognised message type.
    MalformedPayload   = 0x01, ///< Payload does not match expected format.
    VersionMismatch    = 0x02, ///< Protocol version not supported.
    AuthFailed         = 0x03, ///< Authentication token rejected.
    CharacterNotFound  = 0x04, ///< character_id does not exist.
    DaemonOverloaded   = 0x05, ///< Daemon cannot accept more connections.
    InternalError      = 0x06, ///< Unspecified internal daemon error.
};

/**
 * @brief Reason given in a Goodbye frame.
 * @see MSG_GOODBYE
 */
enum class GoodbyeReason : quint8 {
    UserDisconnect  = 0x00, ///< Clean user-initiated disconnect.
    ProtocolError   = 0x01, ///< Disconnecting due to protocol error.
    ShuttingDown    = 0x02, ///< Daemon or GUI is shutting down.
};

/// Bit flag in ServerHello server_flags field: daemon requires authentication.
static constexpr quint8 SERVER_FLAG_AUTH_REQUIRED = 0x01;
