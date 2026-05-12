# MeagreMUD -- Architecture Design Document

**Project:** Open source MegaMUD clone in Qt6/CMake, TCP/IP only, targeting GreaterMUD (and MajorMUD) as servers. GPL-3.0.
**Code style:** No ternary operators. All source files pure ASCII (no UTF-8 multibyte sequences in string literals or comments).

---

## Data Hierarchy

```
MudServer -> UserLogin -> MudInstance -> Character
```

- **MudServer:** host, port, display name, `mud_type` enum (GreaterMUD/MajorMUD), quirks in separate key/value table
- **UserLogin:** credentials (plaintext), belongs to a server
- **MudInstance:** PVP/non-PVP world selection, has `instance_selector` for stimulus/response world selection
- **Character:** belongs to an instance, has `auto_start`, `auto_reconnect` flags

**Persistence:** SQLite3 via Qt's QSQLITE driver.

---

## Architecture: Screen-like Daemon/GUI Split

- Daemon runs on embedded target, manages all character sessions, persists when GUI disconnects
- GUI connects to daemon via TCP (even localhost -- no special local path)
- Binary protocol, persistent stream (not request/response packets)
- **ANSI parsing happens in the daemon; GUI receives pre-parsed `StyledRun` data only**
- ANSI stripper is a standalone daemon-side utility serving: trigger matching, session logging (plaintext), scripting engine

---

## Core Design Principle: No Newline Dependency

**The daemon never uses newline (`\n`) as a flush or segmentation trigger anywhere in the pipeline.** MUD prompts (e.g. `[HP:100 MA:50] >`) sit on open lines indefinitely with no `\n` arriving, so any design that waits for a newline will stall the display at the worst possible moment.

The rule throughout the codebase:
- `AnsiParser` flushes a `StyledRun` when an ANSI escape sequence completes -- not on `\n`
- `\n` is just a character that may appear inside a run's text field
- The GUI `TerminalWidget` is responsible for splitting runs into backbuffer lines when it encounters `\n` in run text
- No class, variable, signal, slot, or protocol message is named after "line" in any context implying a flush or segmentation point (exception: `BackbufferLine` in the resync dump, where historical lines are already complete by definition)
- **Exception -- `AnsiStripper`:** The stripper deliberately buffers to `\n` because its consumers (ScriptEngine, PathEngine, SessionLogger) need line-granular input for pattern matching. This is intentional and isolated to the stripper.

---

## Display

- Custom `QWidget` with `paintEvent` -- no `QTextEdit` or HTML
- Circular backbuffer of `Line` / `StyledRun` structs (a `Line` is `QVector<StyledRun>`)
- Backbuffer capacity is configurable: global default -> per-BBS -> per-character override. `NULL` on character means inherit from server. Resolved value is part of `CharacterConfig`.
- The last `Line` in the backbuffer is always **open/in-progress** -- rendered immediately as runs arrive, closed only when a `\n` is encountered within a run
- Two UI modes: single character full tab view, and tiled overview (multiple characters like a video feed grid)
- `QTabWidget` for tab management
- Observer GUIs see full output but have input locked; status bar indicates observer mode
- MeagreMUD status messages rendered with `[MeagreMUD]` prefix in bright blue (palette index 12), clearly distinguished from MUD output

---

## Protocol -- Frame Header (8 bytes, power of 2)

```
[version: 1][msg_type: 1][character_id: 1][flags: 1][sequence: 2][payload_len: 2]
```

- `character_id` 0 = daemon-level messages
- `flags` = reserved, future use, set to 0
- `sequence` = monotonic counter, rollover detectable
- Max 16 characters (1 byte character_id, 0 reserved, 1-16 characters)

---

## StyledRun Wire Format

```
[fg: 1][bg: 1][bold: 1][text_len: 2][text: N (UTF-8)]
```

- `fg`/`bg` = palette index into 16-colour ANSI palette (8 normal + 8 bright), `0xFF` = default

---

## Protocol -- Message Catalogue

### 0x00-0x0F Handshake/Session
```
0x00 ClientHello     [protocol_version: 1][client_flags: 1][auth_token_len: 1][auth_token: N]
0x01 ServerHello     [protocol_version: 1][server_flags: 1][num_characters: 1]
0x02 ServerReject    [reason_code: 1][reason_len: 1][reason: N]
0x03 Ping            []
0x04 Pong            []
0x05 Goodbye         [reason_code: 1]
0x06 ResyncRequest   []
0x07 ResyncAck       []  (followed immediately by full state dump)
0x08-0x0F reserved
```

`server_flags`: bit `0x01` = auth_required. `client_flags`: reserved, set to 0.

**Ping/Pong:** Both sides send pings; the other side must reply with Pong. The GUI sends pings every 30 seconds. The daemon does the same via `GuiConnection`. A missing pong after timeout disconnects the peer. `MSG_PING` and `MSG_GOODBYE` are handled internally by `DaemonConnectionManager` and never reach `MainWindow`.

### 0x10-0x1F Character State/Dump
```
0x10 CharacterInfo     [name_len: 1][name: N][server_profile_id: 1][instance_id: 1][status: 1]
0x11 BackbufferBegin   [num_lines: 2]
0x12 BackbufferLine    [num_runs: 1][StyledRun x num_runs]
0x13 BackbufferEnd     []
0x14 CharacterInfoEnd  []
0x15-0x1F reserved
```

`BackbufferLine` is line-granular only in the resync dump context. Open/in-progress last line included as-is.

**Character status enum:**
```
0x00 Disconnected
0x01 Connecting
0x02 Connected
0x03 Reconnecting
0x04 Suspended
0x05 Error
```

### 0x20-0x2F Incremental Stream
```
0x20 StyledRun      [StyledRun]
0x21 StatusChange   [status: 1]
0x22 InputEcho      [source: 1][text_len: 2][text: N (UTF-8)]
0x23 ServerMessage  [severity: 1][text_len: 2][text: N (UTF-8)]
0x24-0x2F reserved
```

`InputEcho` source: `0x00` Human, `0x01` Script.
`ServerMessage` severity: `0x00` Info, `0x01` Warning, `0x02` Error.

### 0x30-0x3F Control Messages

**Control is per-daemon, not per-character.**

```
0x30 ControlStatus    [has_control: 1]
0x31 RequestControl   []
0x32 ControlGranted   []
0x33 ControlRevoked   []
0x34 ControlDenied    [reason_code: 1]
0x35-0x3F reserved
```

`ControlDenied` reason: `0x00` AlreadyController, `0x01` NotFound

**Control arbitration:**
- First GUI to complete resync becomes controller; subsequent GUIs are observers
- On controller disconnect, oldest remaining GUI (lowest connectionId) is promoted
- No input frames accepted until resync complete

**UI lockout:** All inputs disabled from connect until `ResyncAck`. Disconnect always available.

### 0x40-0x4F Scripting/Automation

**GUI -> Daemon:**
```
0x40 SetEngineMode    [character_id: 1][mode: 1]
0x41 SetActivePath    [character_id: 1][path_id: 2]
0x42 PreflightRequest [character_id: 1][path_id: 2]
0x43 ClearAttention   [character_id: 1][event_id: 2]
0x44 StepOverride     [character_id: 1]
0x45 AbortPath        [character_id: 1]
0x46 SetCombatFlag    [character_id: 1][flag: 1]
0x47 StatsReset       [character_id: 1]
```

**Engine mode enum:**
```
0x00 Off        <- all automation disabled
0x01 SemiAuto   <- reactive triggers only, no path engine
0x02 Auto       <- path engine active + reactive triggers
```

**Combat flag:** `0x00` Passive, `0x01` Attack

**Daemon -> GUI:**
```
0x48 EngineStatus     [character_id: 1][mode: 1][combat_flag: 1][path_id: 2][current_step: 2][engine_state: 1]
0x49 PreflightResult  [character_id: 1][path_id: 2][pass: 1][issue_count: 1][issues: N]
0x4A AttentionEvent   [character_id: 1][event_id: 2][event_type: 1][text_len: 2][text: N]
0x4B AttentionCleared [character_id: 1][event_id: 2]
0x4C RoomIdentified   [character_id: 1][room_id: 2][room_name_len: 1][room_name: N]
0x4D StatsUpdate      [character_id: 1][payload: 116 bytes fixed]
0x4E-0x4F reserved
```

**Engine state enum:**
```
0x00 Idle
0x01 Running
0x02 Paused          <- combat or waiting
0x03 Retrying        <- room verify failed
0x04 AwaitingHuman   <- attention event raised
0x05 PreflightFailed
0x06 Fleeing
0x07 Resting
0x08 Meditating
0x09 KnockedDown
0x0A Blind
```

**`PreflightResult` issues** packed as repeated `[issue_type: 1][step_order: 2][value_required: 4][value_current: 4]`.
Issue types: gold requirement, level requirement, missing required item (warning not failure).

**`StatsUpdate` payload (116 bytes fixed):**
```
[circuit_count: 2][current_lap: 2]
[gold_gained: 8][exp_gained: 8]
[monsters_killed: 4]
[hit_count: 4][miss_count: 4][times_attacked: 4][times_fled: 4]
[time_fighting: 8][time_resting_hp: 8][time_resting_mana: 8][time_resting_both: 8]
[time_meditating: 8][time_moving: 8][time_idle: 8]
[hp_regen_active: 4][hp_regen_rest: 4]
[mana_regen_active: 4][mana_regen_rest: 4][mana_regen_meditate: 4]
[elapsed_ms: 8][reset_timestamp: 8]
```

### 0x50-0x5F Recording
```
0x50 RecordingStart   [character_id: 1]
0x51 RecordingStep    [character_id: 1][command_len: 1][command: N][room_id: 2]
0x52 RecordingStop    [character_id: 1]
0x53-0x5F reserved
```

`room_id` is `0x0000` for commands that did not produce a room transition. All commands sent during recording are forwarded as `RecordingStep` messages regardless of type.

### 0xF0-0xFF Errors and Diagnostics
```
0xF0 ProtocolError    [error_code: 1][text_len: 2][text: N]
0xF1 DiagnosticInfo   [text_len: 2][text: N]
0xF2 FatalError       [error_code: 1][text_len: 2][text: N]
0xF3 DebugDumpRequest []
0xF4 DebugDumpBegin   []
0xF5 DebugDumpLine    [text_len: 2][text: N]
0xF6 DebugDumpEnd     []
0xF7-0xFF reserved
```

**Error codes:** `0x00` UnknownMessageType, `0x01` MalformedPayload, `0x02` VersionMismatch,
`0x03` AuthFailed, `0x04` CharacterNotFound, `0x05` DaemonOverloaded, `0x06` InternalError

**Protocol error handling:** On `ProtocolError` GUI sends `ResyncRequest`, displays `[MeagreMUD] Protocol resync #N`.

---

## KVM-like Control Model

- First GUI to attach gets control; subsequent GUIs are observers
- Both input areas enqueue into the same `QQueue<QString>`, purely FIFO
- Observer GUIs see all output but input frames are silently discarded by `GuiConnection`

---

## GUI Connection States

```
Disconnected -> Connecting -> Syncing -> Connected
                                 ^
                      (re-enters on ProtocolError)
```

Disconnect/abort during any state sends `Goodbye`, closes socket, returns to `Disconnected`. Daemon keeps all character sessions running.

**Socket error handling:** `QAbstractSocket::ConnectionRefusedError`, `NetworkError`, `SocketTimeoutError`, and `HostNotFoundError` do not fire `disconnected()` after `errorOccurred()`. `DaemonConnectionManager::onSocketError()` explicitly transitions to `Disconnected` for these cases so retry and status bar logic behave correctly.

**Auto-connect retry:** When auto-connect is enabled and a connection fails unexpectedly (not user-initiated), `MainWindow` schedules a retry after `RETRY_INTERVAL_MS` (5000ms). A 1-second countdown timer (`m_countdownTimer`) updates the status bar each tick. Both timers are cancelled on explicit user disconnect, successful connection, or when auto-connect is toggled off. `m_userDisconnected` flag distinguishes user-initiated from unexpected disconnects.

---

## QSettings -- GUI-local Persistent State

Stored in `MeagreMUD.ini` (INI format, user scope). Separate from `meagremud.db`. Contains only machine-local GUI preferences.

```ini
[profiles]
list=Default,Home Server

[profile-Default]
host=127.0.0.1
port=7777
auth_required=false
auth_token=

[profile-Home Server]
host=192.168.1.10
port=7777
auth_required=true
auth_token=...

[gui]
last_profile=Default
auto_connect=true
```

- `profiles/list` -- ordered list of connection profile names
- `profile-{name}/` group -- host, port, auth_required, auth_token per profile
- `gui/last_profile` -- profile used for last successful connection; used by Quick Connect
- `gui/auto_connect` -- whether to auto-connect on launch with the last profile

All other configuration is stored in `meagremud.db` and managed by the daemon.

---

## Database Schema (SQLite3)

### Two-database architecture

**`seed.db`** -- read-only, ships with MeagreMUD, produced by `meagre-scrape`. Contains seeded world knowledge tables and a `SeedMeta` table.

**`meagremud.db`** -- read/write user database. On startup `DatabaseManager` compares `SeedMeta.seed_version` from `seed.db` against `Meta.applied_seed_version` -- if they differ, all `Seeded_*` tables are dropped and repopulated silently. User tables are never touched.

**SQLite views** `v_Room`, `v_Monster`, `v_Item`, `v_RoomExit`, `v_RoomMonster` merge both layers -- user record wins over seeded record where `source_id` matches. `DatabaseManager` is the sole class aware of the dual-table structure.

### seed.db tables

```sql
SeedMeta (seed_version TEXT, generated_at DATETIME, source_version TEXT)
Seeded_Room, Seeded_Monster, Seeded_Item, Seeded_RoomExit, Seeded_RoomMonster
-- same structure as user tables below, source_id always populated
```

### meagremud.db tables

```sql
Meta (key TEXT PRIMARY KEY, value TEXT)
-- keys: applied_seed_version, db_schema_version

MudServer       (id, name, host, port, mud_type, backbuffer_lines, created_at, updated_at)
MudServerQuirk  (id, server_id, quirk_key, quirk_value)
-- quirk keys: meagre_handshake_timeout_ms (default 5000)
--             rest_timeout_ms
--             native_db_version (default '1.11p')

UserLogin       (id, server_id, username, password, created_at, updated_at)
MudInstance     (id, login_id, name, instance_selector, created_at, updated_at)

Character       (id, instance_id, name, password,
                 auto_start, auto_reconnect,
                 backbuffer_lines_override,
                 manual_response_timeout_ms,
                 rest_threshold_hp_pct, rest_threshold_mana_pct,
                 resume_threshold_hp_pct, resume_threshold_mana_pct,
                 meditation_enabled,
                 request_heal_threshold_hp_pct,
                 flee_steps,
                 encumbrance_limit, gold_collection_limit,
                 bank_threshold_copper, bank_room_id,
                 post_combat_command, post_combat_hp_pct,
                 blindness_mode, blindness_cure_command,
                 auto_suspend_exp_per_hour, auto_suspend_check_after_ms,
                 created_at, updated_at)

LoginSequence   (id, character_id, step_order, stimulus, response, is_template)
SessionLog      (id, character_id, started_at, ended_at, log_path)

DisplaySettings (id, owner_type, owner_id, setting_key, setting_value)
-- owner_type: 0=global, 1=MudServer, 2=Character
-- toolbar_buttons key (owner_type=0): comma-separated ordered toolbar button IDs

KeyMacro        (id, owner_type, owner_id, key_code, modifiers, sends_text)

TriggerGroup    (id, character_id, name, enabled)
Trigger         (id, character_id, group_id, pattern, threshold_capture,
                 threshold_op, threshold_value, response,
                 enable_group_id, disable_group_id, priority, enabled)
PartyTrigger    (id, character_id, member_status, client_type_filter, response, enabled)

Room            (id, server_id, name, fingerprint, zone,
                 is_verified, is_safe, source_id, created_at, updated_at)
                 -- fingerprint and is_verified never overwritten by seed reload
RoomExit        (id, room_id, direction, inverse_direction,
                 destination_room_id, is_hidden, created_at)
Monster         (id, server_id, name, disposition, min_level, max_level,
                 notes, source_id, created_at, updated_at)
RoomMonster     (id, room_id, monster_id, spawn_chance)
Item            (id, server_id, name, weight_units, value_copper, is_collectible,
                 currency_value_copper, currency_denomination, notes,
                 source_id, created_at, updated_at)

Path            (id, server_id, name, start_room_id, end_room_id,
                 is_circuit, created_at, updated_at)
PathStep        (id, path_id, step_order, command, expected_room_id,
                 relearn, retry_step, retry_limit,
                 gold_required, level_required, required_item_ids, wait_ms,
                 is_stash_point, is_search_point, is_bank_point, is_rest_point,
                 created_at)
CharacterPath   (id, character_id, path_id, override_active, created_at, updated_at)
CharacterPathStep (id, character_path_id, step_order, command, expected_room_id,
                   relearn, retry_step, retry_limit, gold_required, level_required,
                   required_item_ids, wait_ms,
                   is_stash_point, is_search_point, is_bank_point, is_rest_point)

CharacterSoughtItem (id, character_id, item_id, min_quantity, max_quantity,
                     collect_from_room, collect_from_search)
CharacterStashItem  (id, character_id, item_id, keep_quantity)
AttentionEvent      (id, character_id, event_type, description,
                     step_id, resolved, created_at)
```

---

## CMake Project Structure

```
MeagreMUD/
+-- CMakeLists.txt                          Qt6 (Core Network Sql Svg Widgets)
+-- cmake/MeagreMUDVersion.cmake
+-- meagrelib/                              Static library; used by daemon + scraper
|   +-- CMakeLists.txt
|   +-- src/
|       +-- protocol/Protocol.h            Message type constants, enums
|       +-- types/MudTypes.h               StyledRun, Line, CharacterStatus, EngineMode...
|       +-- db/
|           +-- Schema.h                   All SQL DDL as constexpr strings
|           +-- DatabaseManager.h / .cpp   Dual-DB manager (seed.db + meagremud.db)
+-- daemon/
|   +-- CMakeLists.txt
|   +-- src/
|       +-- main.cpp
|       +-- DaemonServer.h / .cpp
|       +-- GuiConnection.h / .cpp
|       +-- CharacterSession.h / .cpp
|       +-- CharacterRegistry.h / .cpp
|       +-- AnsiParser.h / .cpp
|       +-- AnsiStripper.h / .cpp
|       +-- SessionLogger.h / .cpp
|       +-- ScriptEngine.h / .cpp          (not yet implemented)
|       +-- PathEngine.h / .cpp            (not yet implemented)
+-- gui/
|   +-- CMakeLists.txt                     Qt6 Svg required for IconFactory
|   +-- src/
|       +-- main.cpp
|       +-- connection/
|       |   +-- DaemonConnectionManager.h / .cpp
|       +-- window/
|       |   +-- MainWindow.h / .cpp
|       |   +-- AttentionPanel.h / .cpp
|       |   +-- TiledArea.h / .cpp
|       +-- pane/
|       |   +-- CharacterPane.h / .cpp
|       |   +-- StatusLine.h / .cpp
|       +-- terminal/
|       |   +-- TerminalWidget.h / .cpp
|       +-- input/
|       |   +-- InputWidget.h / .cpp
|       +-- dialogs/
|       |   +-- ConnectionDialog.h / .cpp
|       |   +-- SettingsDialog.h / .cpp
|       +-- toolbar/
|           +-- IconFactory.h / .cpp
|           +-- ToolbarManager.h / .cpp
+-- scraper/
|   +-- CMakeLists.txt
|   +-- src/
|       +-- main.cpp
|       +-- BtrieveReader.h / .cpp
|       +-- NativeDbImporter.h / .cpp
|       +-- WorldKnowledgeWriter.h / .cpp
+-- docs/
|   +-- Doxyfile
|   +-- mainpage.md
|   +-- MeagreMUD-Architecture.md
+-- test/
    +-- CMakeLists.txt
    +-- networkTest/
```

---

## Daemon Implementation Structure

### Threading model
- **Main thread:** `DaemonServer`, `CharacterRegistry`, all `GuiConnection`s
- **Session thread N:** `CharacterSession`, `AnsiParser`, `AnsiStripper`, `ScriptEngine`, `PathEngine`, `SessionLogger`, `QTcpSocket` (MUD side)
- One `QThread` per `CharacterSession`
- `QTcpSocket` created in `CharacterSession::init()` -- never in constructor
- Cross-thread signals use Qt automatic queued connection

### Component responsibilities

**`AnsiParser`** -- SGR server->client only. Emits `runReady(StyledRun)` on escape completion. `\n` never triggers flush. SGR codes: 0=reset, 1=bold, 30-37=fg normal, 39=fg default, 40-47=bg normal, 49=bg default, 90-97=fg bright, 100-107=bg bright. Non-SGR: `ESC[2J`=screenCleared(), `ESC[H`=drop, `\r`=pendingCR.

**`AnsiStripper`** -- receives `StyledRun` stream, strips style, buffers to `\n`, emits `strippedText(QString)`. `flush()` drains pending buffer on disconnect. `reset()` clears buffer on reconnect.

**`SessionLogger`** -- receives `strippedText(QString)`, opens log file lazily, flushes after every write, creates directories as needed. `stop()` closes file on session end. Optional per-line timestamps.

**`ScriptEngine`** -- evaluates triggers, maintains tracked variables (HP, mana, exp, gold, encumbrance). Consumes `@meagre` telepath silently. Also receives raw `StyledRun` for colour-based monster disposition detection.

**`PathEngine`** -- manages Off/SemiAuto/Auto modes and Passive/Attack combat flag. Owns path history ring buffer for flee. Manages rest/meditate decision logic. Tracks session statistics.

### Signal/slot flow
```
MUD bytes -> AnsiParser::onData()
    | runReady(StyledRun)
    +---> CharacterSession::onRunReady()
    |         | runReady(charId, StyledRun) [queued]
    |         v DaemonServer::broadcastStyledRun()
    |           -> GuiConnection::sendFrame() [x N GUIs]
    |
    +---> AnsiStripper::onRunReady()
              | strippedText(QString)
              +---> ScriptEngine::onStrippedText()
              +---> PathEngine::onStrippedText()
              +---> SessionLogger::onStrippedText()

(ScriptEngine also receives runReady(StyledRun) directly for colour inspection)
```

---

## AnsiParser State Machine

```
Normal:      0x1B -> EscapeStart, \r -> pendingCR, other -> append to run
EscapeStart: '[' -> CSI, other -> drop ESC, re-process char
CSI:         digits/';' -> CSIParams, 'H' -> drop, 'm' -> SGR reset, '2' -> CSIErase, other -> drop
CSIParams:   digits/';' -> accumulate, 'm' -> parse SGR emit run apply style, other -> drop
CSIErase:    'J' -> screenCleared(), other -> drop
```

`pendingCR`: next `\n` -> append `\r\n`; else -> append `\r`, process byte.

---

## Three Operating Modes + Combat Flag

| Mode | Path Engine | Reactive Triggers | Human Input |
|---|---|---|---|
| Off | Inactive | Inactive | Full control |
| SemiAuto | Inactive | Active | Full control |
| Auto | Active | Active | At own risk |

**Combat flag:**
- **Passive:** Never initiate, never retaliate -- keep moving.
- **Attack:** Engage hostiles immediately. Engage neutrals if clearing room.

**Monster disposition from ANSI colour (live colour always overrides DB):**
- Pink/Magenta = Hostile, Green/Grey = Neutral, White/Bold = Friendly, Unknown = react only

---

## Path Engine State Machine

```
Idle -> PreflightCheck -> fail -> PreflightFailed (-> Idle)
                        -> pass -> Running
Running:
    verify room fingerprint on arrival
    match -> advance step
    unknown -> AttentionEvent, advance anyway
    mismatch -> jump retry_step; retry_limit exceeded -> AwaitingHuman
    combat fires -> Paused -> (combat ends) -> Running
    flee condition -> send flee_steps commands rapidly -> verify -> rest if clear
    AbortPath -> Idle
    circuit complete -> loop
    point-to-point complete -> Idle
```

All laps run with the configured combat flag. No special-case first-lap logic.

**Flee:** Retrace path history (inverse directions). Stop early at `is_safe` room. `inverse_direction` NULL = skip step.

---

## Rest / Meditate Decision Logic

```
hp_deficit   = resume_threshold_hp_pct   - hp_current_pct   (0 if at/above)
mana_deficit = resume_threshold_mana_pct - mana_current_pct (0 if at/above)

if hp_deficit == 0 and mana_deficit == 0: stand, resume
if meditation_enabled and mana_deficit > hp_deficit: meditate
else: rest

Re-evaluate on each HP/mana update while stopped; switch between rest/meditate as needed.
```

`is_rest_point` on a `PathStep` triggers rest/meditate before proceeding regardless of thresholds.

---

## Statistics System

Pure in-memory per `CharacterSession`. No DB persistence. Lost on session end.

**Tracked:** Circuit count, lap, gold/exp gained, monsters killed, hit/miss/attacked/fled, time breakdown, HP/mana regen rates per engine state.

**Auto-suspend:** If `auto_suspend_exp_per_hour` set and exp/hour drops below threshold -- suspend, emit Warning, add AttentionEvent.

---

## Inventory / Economy System

**Currency collection priority:** Collect highest denomination first. Never drop before picking up.

**Bank deposit:** `deposit_amount = gold_carried - toll_cost_to_return - cash_reserve_copper`. Skip if <= 0.

**Stash points:** Drop items in `CharacterStashItem` (keeping `keep_quantity`) when passing `is_stash_point` steps.

**Search points:** Send `search\r\n` at `is_search_point` steps; collect `CharacterSoughtItem` up to `max_quantity`.

**Pre-flight:** Missing required items and below-min sought items are non-blocking warnings.

---

## GUI Implementation

### MainWindow layout
```
MainWindow
+-- QToolBar (ToolbarManager -- context-sensitive, always visible)
+-- QMenuBar
+-- TiledArea (hidden when empty)
+-- AttentionPanel (collapsible drawer)
+-- QTabWidget (docked character panes)
+-- QStatusBar (permanent QLabel + transient showMessage)
```

### Toolbar -- ToolbarManager

Always visible. Content changes based on GUI-daemon connection state.

**Pre-daemon-connection** (reuses File menu QAction instances -- same object appears in both):
```
[Connection...] [Quick Connect] [Auto-connect] | [Disconnect]
```

**Post-daemon-connection:**
```
[BBS toggle] | [Go To] [Circuit] [Cease] | [Automation master]
[Combat] [Rest] [Bless] [Get Items] [Get Money] [Sneak] [Hide] [Search] | [Settings]
```

Icons generated by `IconFactory` from inline SVG strings at 24x24 and 48x48 (HiDPI). No external files, no `.qrc` resource file. Requires `Qt6::Svg`.

**BBS toggle** (`ID_BBS_CONNECTION`) -- single checkable `QAction`. Checked = character connected to BBS. `setBbsConnected()` updates checked state with signals blocked.

**Automation master** -- disables child actions when unchecked but preserves their checked state. Re-enabling restores previous child states.

**Toolbar button IDs** (for DB serialisation in `DisplaySettings`):
```
bbs_connection, goto_location, run_circuit, cease,
automation, auto_combat, auto_rest, auto_bless, auto_get_items,
auto_get_money, auto_sneak, auto_hide, auto_search, settings
```

Button visibility/order: `DisplaySettings` table, `owner_type=0` (global), key `toolbar_buttons`.

### File menu action states

| State | Connect | Quick Connect | Disconnect | Settings/PathEditor |
|---|---|---|---|---|
| Disconnected | on | on if profile saved | on | off |
| Connecting | on | on if profile saved | on | off |
| Syncing | on | on if profile saved | on | off |
| Connected | on | on if profile saved | on | on |

### ConnectionDialog

Profile-based connection dialog. Stored in `MeagreMUD.ini`.

- **OK button** label: "Save && Connect" when Disconnected, "Save" when not.
- **Connect / Disconnect / Save** buttons reflect `DaemonConnectionManager::State` passed at construction.
- `saveLastProfile()` / `lastUsedProfile()` are public static methods.
- `saveLastProfile()` called whenever `connectRequested` is emitted.

### SettingsDialog

Six-tab dialog. Tabs 1-5 are stubs. Only available when Connected (daemon-backed). Tab 6 (Daemon Connection) superseded by `ConnectionDialog`.

### CharacterPane
`TerminalWidget` + `StatusLine` + `InputWidget`. Reparented on dock/undock without state loss.

### TerminalWidget
Circular backbuffer. `\n` closes lines. `\r` overwrites open line. Mouse wheel + Page Up/Down scrollback. Auto-follows live output unless user has scrolled up. 16-colour ANSI palette. Runtime-configurable font (must be monospaced).

### StatusLine
QLabel bar: character name | server | status (colour-coded) | engine mode | combat flag | room name | script indicator.

### InputWidget
100-entry history ring, Up/Down navigation with draft preservation, key macro interception, `\r\n` appended on submit, locked during resync/observer.

### AttentionPanel
Collapsible drawer. Expands on first event. Dismiss sends `0x43 ClearAttention`.

---

## Path/Circuit Editor

Non-modal `QWidget` from Tools menu. Single instance.

```
PathEditor
+-- QSplitter
    +-- Left:  QTreeWidget (Circuits/P2P) + [New] [Delete] [Rename] [Import CSV]
    +-- Right: Header bar + Recording toolbar + QTabWidget (Steps | Map)
```

**Recording:** All commands appended verbatim. `room_id` auto-fills Expected Room for movement commands.

**PathStep flags:** `relearn`, `is_rest_point`, `is_stash_point`, `is_search_point`, `is_bank_point`.

**Map tab:** ASCII ~21x21 grid. `*` room, `+` junction, `[*]` on-path, `|` N/S, `-` E/W, `/` NE/SW, `\` NW/SE, `^`/`v` up/down. Click-to-build. Z-level navigation. Unverified rooms dimmed.

**CSV import/export:** `map/room` notation for expected_room. Import resolves via `v_Room`.

---

## World Knowledge -- NativeDbImporter

MajorMUD v1.11p Btrieve `.dat` files. `BtrieveReader` walks raw binary (no DLL required). Field offsets from Nightmare Redux `Sub IntFieldMaps`. All offset knowledge private to `NativeDbImporter.cpp`.

Files: `w<CC>mp002.dat` (rooms), `w<CC>knms2.dat` (monsters), `w<CC>knit2.dat` (items). `<CC>` from `native_db_version` quirk.

**Usage:** `meagre-scrape import --db <seed.db> --server-id <id> <source_dir>`

---

## @meagre Telepath Protocol

Format: `tell <name> @meagre <version> <command> [params]`. Version `1`. Plain ASCII. Consumed silently by ScriptEngine. Pattern: `* tells you '@meagre # *'`.

**Capability bitmask:** `0x01` cure_blind, `0x02` heal_single, `0x04` heal_party, `0x08` bless, `0x10` boost_party, `0x20` cure_poison, `0x40` resurrect (future).

Handshake: `hello` -> `ok <version> caps:<bitmask>` -> `request caps_detail` -> `caps_detail <cap>:<cmd>...`. Timeout from `meagre_handshake_timeout_ms`. No reply -> try MegaMUD `@status`. Still no reply -> Manual.

---

## Party System

Party state is transient -- not persisted. State updated via periodic `party` command poll + real-time server message parsing.

Capabilities: cure_blind, heal_single, heal_party, bless, boost_party, cure_poison.
Member statuses: Active, KnockedOut, Separated, Unknown.
Client types: Unknown, MeagreMUD, MegaMUD, Manual.

Cure/heal: leader selects first available capable member (not in combat, mana > threshold). Single request at a time.

Pre-rest sequence: request buffs/blessings -> request heals -> rest/meditate -> on resume post_combat_command.

---

## Blindness Handling

- **0 = Wait:** Suspend movement, continue combat, wait for vision clears
- **1 = Ignore:** Keep moving; no fingerprint verification
- **2 = CureSpell:** Send `blindness_cure_command`; fall back to Wait on failure

---

## Knocked Down Handling

Sets `KnockedDown` engine state. Suppresses movement until stand-up message. If fleeing when knocked down -- waits until standing then resumes.

---

## GreaterMUD Notes

GreaterMUD is a MajorMUD fork. Database format assumed identical to v1.11p. GreaterMUD live server is primary target; MajorMUD `seed.db` provides ~99% world coverage. Divergences noted in `NativeDbImporter.cpp` via live play and `relearn`.

---

## Build Instructions

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Doxygen documentation
doxygen docs/Doxyfile
# Output: docs/doxygen/html/index.html
```

Requirements: Qt6 (Core, Network, Sql, Svg, Widgets), CMake 3.22+. Graphviz optional (call graphs).
