#pragma once

#include <QtGlobal>

// ---------------------------------------------------------------------------
// Frame header — 8 bytes
//
//   [version: 1][msg_type: 1][character_id: 1][flags: 1][sequence: 2][payload_len: 2]
//
// character_id 0 = daemon-level messages
// flags        = reserved, set to 0
// sequence     = monotonic counter, rollover detectable
// ---------------------------------------------------------------------------

static constexpr int    FRAME_HEADER_SIZE    = 8;
static constexpr quint8 PROTOCOL_VERSION     = 1;
static constexpr quint8 CHARACTER_ID_DAEMON  = 0x00;

// ---------------------------------------------------------------------------
// Message type catalogue
// ---------------------------------------------------------------------------

// 0x00–0x0F  Handshake / Session
static constexpr quint8 MSG_CLIENT_HELLO     = 0x00;
static constexpr quint8 MSG_SERVER_HELLO     = 0x01;
static constexpr quint8 MSG_SERVER_REJECT    = 0x02;
static constexpr quint8 MSG_PING             = 0x03;
static constexpr quint8 MSG_PONG             = 0x04;
static constexpr quint8 MSG_GOODBYE          = 0x05;
static constexpr quint8 MSG_RESYNC_REQUEST   = 0x06;
static constexpr quint8 MSG_RESYNC_ACK       = 0x07;

// 0x10–0x1F  Character State / Dump
static constexpr quint8 MSG_CHARACTER_INFO      = 0x10;
static constexpr quint8 MSG_BACKBUFFER_BEGIN     = 0x11;
static constexpr quint8 MSG_BACKBUFFER_LINE      = 0x12;
static constexpr quint8 MSG_BACKBUFFER_END       = 0x13;
static constexpr quint8 MSG_CHARACTER_INFO_END   = 0x14;

// 0x20–0x2F  Incremental Stream
static constexpr quint8 MSG_STYLED_RUN       = 0x20;
static constexpr quint8 MSG_STATUS_CHANGE    = 0x21;
static constexpr quint8 MSG_INPUT_ECHO       = 0x22;
static constexpr quint8 MSG_SERVER_MESSAGE   = 0x23;

// 0x30–0x3F  Control
static constexpr quint8 MSG_CONTROL_STATUS   = 0x30;
static constexpr quint8 MSG_REQUEST_CONTROL  = 0x31;
static constexpr quint8 MSG_CONTROL_GRANTED  = 0x32;
static constexpr quint8 MSG_CONTROL_REVOKED  = 0x33;
static constexpr quint8 MSG_CONTROL_DENIED   = 0x34;

// 0x40–0x4F  Scripting / Automation
static constexpr quint8 MSG_SET_ENGINE_MODE      = 0x40;
static constexpr quint8 MSG_SET_ACTIVE_PATH      = 0x41;
static constexpr quint8 MSG_PREFLIGHT_REQUEST    = 0x42;
static constexpr quint8 MSG_CLEAR_ATTENTION      = 0x43;
static constexpr quint8 MSG_STEP_OVERRIDE        = 0x44;
static constexpr quint8 MSG_ABORT_PATH           = 0x45;
static constexpr quint8 MSG_SET_COMBAT_FLAG      = 0x46;
static constexpr quint8 MSG_STATS_RESET          = 0x47;
static constexpr quint8 MSG_ENGINE_STATUS        = 0x48;
static constexpr quint8 MSG_PREFLIGHT_RESULT     = 0x49;
static constexpr quint8 MSG_ATTENTION_EVENT      = 0x4A;
static constexpr quint8 MSG_ATTENTION_CLEARED    = 0x4B;
static constexpr quint8 MSG_ROOM_IDENTIFIED      = 0x4C;
static constexpr quint8 MSG_STATS_UPDATE         = 0x4D;

// 0x50–0x5F  Recording
static constexpr quint8 MSG_RECORDING_START      = 0x50;
static constexpr quint8 MSG_RECORDING_STEP       = 0x51;
static constexpr quint8 MSG_RECORDING_STOP       = 0x52;

// 0xF0–0xFF  Errors / Diagnostics
static constexpr quint8 MSG_PROTOCOL_ERROR       = 0xF0;
static constexpr quint8 MSG_DIAGNOSTIC_INFO      = 0xF1;
static constexpr quint8 MSG_FATAL_ERROR          = 0xF2;
static constexpr quint8 MSG_DEBUG_DUMP_REQUEST   = 0xF3;
static constexpr quint8 MSG_DEBUG_DUMP_BEGIN     = 0xF4;
static constexpr quint8 MSG_DEBUG_DUMP_LINE      = 0xF5;
static constexpr quint8 MSG_DEBUG_DUMP_END       = 0xF6;

// ---------------------------------------------------------------------------
// Sub-type enumerations
// ---------------------------------------------------------------------------

// Character status
enum class CharacterStatus : quint8 {
    Disconnected = 0x00,
    Connecting   = 0x01,
    Connected    = 0x02,
    Reconnecting = 0x03,
    Suspended    = 0x04,
    Error        = 0x05,
};

// Engine mode
enum class EngineMode : quint8 {
    Off      = 0x00,
    SemiAuto = 0x01,
    Auto     = 0x02,
};

// Combat flag
enum class CombatFlag : quint8 {
    Passive = 0x00,
    Attack  = 0x01,
};

// Engine state
enum class EngineState : quint8 {
    Idle            = 0x00,
    Running         = 0x01,
    Paused          = 0x02,
    Retrying        = 0x03,
    AwaitingHuman   = 0x04,
    PreflightFailed = 0x05,
    Fleeing         = 0x06,
    Resting         = 0x07,
    Meditating      = 0x08,
    KnockedDown     = 0x09,
    Blind           = 0x0A,
};

// ServerMessage severity
enum class MessageSeverity : quint8 {
    Info    = 0x00,
    Warning = 0x01,
    Error   = 0x02,
};

// InputEcho source
enum class InputSource : quint8 {
    Human  = 0x00,
    Script = 0x01,
};

// ControlDenied reason
enum class ControlDeniedReason : quint8 {
    AlreadyController = 0x00,
    NotFound          = 0x01,
};

// Protocol error codes
enum class ProtocolErrorCode : quint8 {
    UnknownMessageType = 0x00,
    MalformedPayload   = 0x01,
    VersionMismatch    = 0x02,
    AuthFailed         = 0x03,
    CharacterNotFound  = 0x04,
    DaemonOverloaded   = 0x05,
    InternalError      = 0x06,
};

// Goodbye reason codes
enum class GoodbyeReason : quint8 {
    UserDisconnect  = 0x00,
    ProtocolError   = 0x01,
    ShuttingDown    = 0x02,
};

// ServerHello flags
static constexpr quint8 SERVER_FLAG_AUTH_REQUIRED = 0x01;
