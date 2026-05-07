# MeagreMUD — Architecture Design Document

**Project:** Open source MegaMUD clone in Qt6/CMake, TCP/IP only, targeting GreaterMUD (and MajorMUD) as servers. GPL-3.0.
**Code style:** No ternary operators.

---

## Data Hierarchy

```
MudServer → UserLogin → MudInstance → Character
```

- **MudServer:** host, port, display name, `mud_type` enum (GreaterMUD/MajorMUD), quirks in separate key/value table
- **UserLogin:** credentials (plaintext), belongs to a server
- **MudInstance:** PVP/non-PVP world selection, has `instance_selector` for stimulus/response world selection
- **Character:** belongs to an instance, has `auto_start`, `auto_reconnect` flags

**Persistence:** SQLite3 via Qt's QSQLITE driver.

---

## Architecture: Screen-like Daemon/GUI Split

- Daemon runs on embedded target, manages all character sessions, persists when GUI disconnects
- GUI connects to daemon via TCP (even localhost — no special local path)
- Binary protocol, persistent stream (not request/response packets)
- **ANSI parsing happens in the daemon; GUI receives pre-parsed `StyledRun` data only**
- ANSI stripper is a standalone daemon-side utility serving: trigger matching, session logging (plaintext), scripting engine

---

## Core Design Principle: No Newline Dependency

**The daemon never uses newline (`\n`) as a flush or segmentation trigger anywhere in the pipeline.** MUD prompts (e.g. `[HP:100 MA:50] >`) sit on open lines indefinitely with no `\n` arriving, so any design that waits for a newline will stall the display at the worst possible moment.

The rule throughout the codebase:
- `AnsiParser` flushes a `StyledRun` when an ANSI escape sequence completes — not on `\n`
- `\n` is just a character that may appear inside a run's text field
- The GUI `TerminalWidget` is responsible for splitting runs into backbuffer lines when it encounters `\n` in run text
- No class, variable, signal, slot, or protocol message is named after "line" in any context implying a flush or segmentation point (exception: `BackbufferLine` in the resync dump, where historical lines are already complete by definition)

---

## Display

- Custom `QWidget` with `paintEvent` — no `QTextEdit` or HTML
- Circular backbuffer of `Line` / `StyledRun` structs (a `Line` is `QVector<StyledRun>`)
- Backbuffer capacity is configurable: global default → per-BBS → per-character override. `NULL` on character means inherit from server. Resolved value is part of `CharacterConfig`.
- The last `Line` in the backbuffer is always **open/in-progress** — rendered immediately as runs arrive, closed only when a `\n` is encountered within a run
- Two UI modes: single character full tab view, and tiled overview (multiple characters like a video feed grid)
- `QTabWidget` for tab management
- Observer GUIs grayed out
- MeagreMUD status messages rendered with `[MeagreMUD]` prefix in distinct colour (blue), clearly distinguished from MUD output

---

## Protocol — Frame Header (8 bytes, power of 2)

```
[version: 1][msg_type: 1][character_id: 1][flags: 1][sequence: 2][payload_len: 2]
```

- `character_id` 0 = daemon-level messages
- `flags` = reserved, future use, set to 0
- `sequence` = monotonic counter, rollover detectable
- Max 16 characters (1 byte character_id, 0 reserved, 1–16 characters)

---

## StyledRun Wire Format

```
[fg: 1][bg: 1][bold: 1][text_len: 2][text: N (UTF-8)]
```

- `fg`/`bg` = palette index into 16-colour ANSI palette (8 normal + 8 bright), `0xFF` = default

---

## Protocol — Message Catalogue

### 0x00–0x0F Handshake/Session
```
0x00 ClientHello     [protocol_version: 1][client_flags: 1][auth_token_len: 1][auth_token: N]
0x01 ServerHello     [protocol_version: 1][server_flags: 1][num_characters: 1]
0x02 ServerReject    [reason_code: 1][reason_len: 1][reason: N]
0x03 Ping            []
0x04 Pong            []
0x05 Goodbye         [reason_code: 1]
0x06 ResyncRequest   []
0x07 ResyncAck       []  (followed immediately by full state dump)
0x08–0x0F reserved
```

`server_flags`: bit `0x01` = auth_required. `client_flags`: reserved, set to 0.

### 0x10–0x1F Character State/Dump
```
0x10 CharacterInfo     [name_len: 1][name: N][server_profile_id: 1][instance_id: 1][status: 1]
0x11 BackbufferBegin   [num_lines: 2]
0x12 BackbufferLine    [num_runs: 1][StyledRun × num_runs]
0x13 BackbufferEnd     []
0x14 CharacterInfoEnd  []
0x15–0x1F reserved
```

**Note:** `BackbufferLine` is line-granular only in the resync dump context — historical lines are already complete. Open/in-progress last line included as-is. Only place line boundaries appear in the protocol.

**Character status enum:**
```
0x00 Disconnected
0x01 Connecting
0x02 Connected
0x03 Reconnecting
0x04 Suspended
0x05 Error
```

### 0x20–0x2F Incremental Stream
```
0x20 StyledRun      [StyledRun]
0x21 StatusChange   [status: 1]
0x22 InputEcho      [source: 1][text_len: 2][text: N (UTF-8)]
0x23 ServerMessage  [severity: 1][text_len: 2][text: N (UTF-8)]
0x24–0x2F reserved
```

`InputEcho` source: `0x00` Human, `0x01` Script — cosmetic only, both travel same input queue.
`ServerMessage` severity: `0x00` Info, `0x01` Warning, `0x02` Error

### 0x30–0x3F Control Messages

**Control is per-daemon, not per-character.**

```
0x30 ControlStatus    [has_control: 1]
0x31 RequestControl   []
0x32 ControlGranted   []
0x33 ControlRevoked   []
0x34 ControlDenied    [reason_code: 1]
0x35–0x3F reserved
```

`ControlDenied` reason: `0x00` AlreadyController, `0x01` NotFound

**Control arbitration:**
- First GUI to connect becomes controller; subsequent GUIs are observers
- On controller disconnect, oldest remaining GUI is promoted
- No `ScriptProtected` — all input is `\r\n` terminated, interleaving impossible by design
- No `RequestControl` or input frames accepted until resync complete

**UI lockout:** All inputs disabled from connect until `ResyncAck`. Single "Disconnect" button available throughout.

### 0x40–0x4F Scripting/Automation

**GUI → Daemon:**
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
0x00 Off        ← all automation disabled
0x01 SemiAuto   ← reactive triggers only, no path engine
0x02 Auto       ← path engine active + reactive triggers
```

**Combat flag:** `0x00` Passive (never initiate, never retaliate — keep moving), `0x01` Attack

**Daemon → GUI:**
```
0x48 EngineStatus     [character_id: 1][mode: 1][combat_flag: 1][path_id: 2][current_step: 2][engine_state: 1]
0x49 PreflightResult  [character_id: 1][path_id: 2][pass: 1][issue_count: 1][issues: N]
0x4A AttentionEvent   [character_id: 1][event_id: 2][event_type: 1][text_len: 2][text: N]
0x4B AttentionCleared [character_id: 1][event_id: 2]
0x4C RoomIdentified   [character_id: 1][room_id: 2][room_name_len: 1][room_name: N]
0x4D StatsUpdate      [character_id: 1][payload: 116 bytes fixed]
0x4E–0x4F reserved
```

**Engine state enum:**
```
0x00 Idle
0x01 Running
0x02 Paused          ← combat or waiting
0x03 Retrying        ← room verify failed
0x04 AwaitingHuman   ← attention event raised
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

### 0x50–0x5F Recording
```
0x50 RecordingStart   [character_id: 1]
0x51 RecordingStep    [character_id: 1][command_len: 1][command: N][room_id: 2]
0x52 RecordingStop    [character_id: 1]
0x53–0x5F reserved
```

`room_id` is `0x0000` for commands that did not produce a room transition. Recording taps the existing input queue — all commands sent by the user during recording are forwarded as `RecordingStep` messages regardless of type. Only one character can be recorded at a time per GUI connection. Recording does not affect normal daemon operation.

### 0xF0–0xFF Errors and Diagnostics
```
0xF0 ProtocolError    [error_code: 1][text_len: 2][text: N]
0xF1 DiagnosticInfo   [text_len: 2][text: N]
0xF2 FatalError       [error_code: 1][text_len: 2][text: N]
0xF3 DebugDumpRequest []
0xF4 DebugDumpBegin   []
0xF5 DebugDumpLine    [text_len: 2][text: N]
0xF6 DebugDumpEnd     []
0xF7–0xFF reserved
```

**Error codes:** `0x00` UnknownMessageType, `0x01` MalformedPayload, `0x02` VersionMismatch,
`0x03` AuthFailed, `0x04` CharacterNotFound, `0x05` DaemonOverloaded, `0x06` InternalError

**Protocol error handling:** On `ProtocolError` GUI sends `ResyncRequest`, displays `[MeagreMUD] Protocol resync #N`. Counter persists for session.

---

## KVM-like Control Model

- First GUI to attach gets control; subsequent GUIs are observers
- Both input areas enqueue into the same `QQueue<QString>`, purely FIFO
- Human overriding bot mid-sequence is their own risk

---

## GUI Connection States

```
Disconnected → Connecting → Syncing → Connected
                               ↑
                    (re-enters on ProtocolError)
```

Disconnect/abort during any state sends `Goodbye`, closes socket, returns to `Disconnected`. Daemon keeps all character sessions running. GUI preserves last known `CharacterPane` state (dimmed) while disconnected.

---

## Database Schema (SQLite3)

### Two-database architecture

**`seed.db`** — read-only, ships with MeagreMUD, produced by `meagre-scrape`. Contains seeded world knowledge tables and a `SeedMeta` table. Installed to a read-only system path (e.g. `/usr/share/meagremud/seed.db`).

**`meagremud.db`** — read/write user database. Contains user layer world knowledge tables, all configuration, and a `Meta` table. On startup `DatabaseManager` compares `SeedMeta.seed_version` from `seed.db` against `Meta.applied_seed_version` in `meagremud.db` — if they differ, all `Seeded_*` tables are dropped and repopulated silently. User tables are never touched by this process.

**SQLite views** `v_Room`, `v_Monster`, `v_Item`, `v_RoomExit`, `v_RoomMonster` merge both layers — user record wins over seeded record where `source_id` matches. All application code above `DatabaseManager` reads through views. Writes always go to user tables. `DatabaseManager` is the sole class aware of the dual-table structure.

---

### seed.db tables

```sql
SeedMeta (
    seed_version    TEXT NOT NULL,
    generated_at    DATETIME NOT NULL,
    source_version  TEXT NOT NULL    -- e.g. '1.11p'
)

Seeded_Room         -- same structure as Room below, source_id always populated
Seeded_Monster      -- same structure as Monster below
Seeded_Item         -- same structure as Item below
Seeded_RoomExit     -- same structure as RoomExit below
Seeded_RoomMonster  -- same structure as RoomMonster below
```

---

### meagremud.db tables

```sql
Meta (key TEXT PRIMARY KEY, value TEXT)
-- keys: applied_seed_version, db_schema_version

-- Core hierarchy
MudServer       (id, name, host, port, mud_type, backbuffer_lines, created_at, updated_at)
MudServerQuirk  (id, server_id, quirk_key, quirk_value)
-- quirk keys: meagre_handshake_timeout_ms (default 5000)
--             rest_timeout_ms (MegaMUD rest timeout)
--             native_db_version (default '1.11p') -- used by NativeDbImporter field offset map

UserLogin       (id, server_id, username, password, created_at, updated_at)
MudInstance     (id, login_id, name, instance_selector, created_at, updated_at)

Character       (id, instance_id, name, password,
                 auto_start, auto_reconnect,
                 backbuffer_lines_override,
                 manual_response_timeout_ms,
                 -- Rest/meditate
                 rest_threshold_hp_pct,
                 rest_threshold_mana_pct,
                 resume_threshold_hp_pct,
                 resume_threshold_mana_pct,
                 meditation_enabled,           -- BOOLEAN
                 -- Party
                 request_heal_threshold_hp_pct,
                 -- Flee
                 flee_steps,                   -- default 3
                 -- Encumbrance
                 encumbrance_limit,            -- 0=Unencumbered 1=Light 2=Medium 3=Heavy
                 gold_collection_limit,        -- 0=always collect 1=stop at encumbrance_limit
                 -- Banking
                 bank_threshold_copper,        -- NULL=never auto-bank
                 bank_room_id,                 -- FK to Room
                 -- Post-combat
                 post_combat_command,          -- e.g. "sneak\r\nhide\r\n", NULL=none
                 post_combat_hp_pct,           -- only execute if HP above this %, NULL=always
                 -- Blindness
                 blindness_mode,               -- 0=Wait 1=Ignore 2=CureSpell
                 blindness_cure_command,       -- NULL if mode != 2
                 -- Auto-suspend
                 auto_suspend_exp_per_hour,    -- NULL=disabled
                 auto_suspend_check_after_ms,  -- default 1800000 (30 min)
                 created_at, updated_at)

LoginSequence   (id, character_id, step_order, stimulus, response, is_template)
SessionLog      (id, character_id, started_at, ended_at, log_path)

-- Display / input settings (three-level inheritance: global → MudServer → Character)
DisplaySettings (id, owner_type, owner_id, setting_key, setting_value)
-- owner_type: 0=global, 1=MudServer, 2=Character
-- Default palette: standard dark ANSI terminal (black bg, light fg, classic 8+8 colours)

KeyMacro        (id, owner_type, owner_id, key_code, modifiers, sends_text)

-- Scripting
TriggerGroup    (id, character_id, name, enabled)

Trigger         (id, character_id, group_id,
                 pattern,              -- wildcard: * = any text, # = any number
                 threshold_capture,    -- which #N to compare, NULL=none
                 threshold_op,         -- '<' or '>'
                 threshold_value,
                 response,
                 enable_group_id,
                 disable_group_id,
                 priority,
                 enabled)

PartyTrigger    (id, character_id,
                 member_status,        -- KnockedOut/Separated/Dead
                 client_type_filter,   -- 0=Any 1=MeagreMUD 2=MegaMUD 3=Manual
                 response,
                 enabled)

-- World knowledge — user layer (seeded records promoted here when user customises them)
-- source_id NULL = user-created record; source_id populated = promoted from seed
Room            (id, server_id,
                 name,
                 fingerprint,          -- normalised: room_name + "|" + obvious_exits_as_received
                                       -- ANSI stripped, whitespace normalised, no sort
                                       -- NOTE: never overwritten by seed reload
                 zone,
                 is_verified,          -- confirmed against live server
                                       -- NOTE: never overwritten by seed reload
                 is_safe,              -- monsters cannot enter
                 source_id,            -- original MajorMUD room ID for traceability; NULL if user-created
                 created_at, updated_at)

RoomExit        (id, room_id,
                 direction,            -- full name e.g. "north", or custom e.g. "go skiff"
                 inverse_direction,    -- explicit e.g. "south", NULL if one-way/unknown
                 destination_room_id,
                 is_hidden,
                 created_at)

Monster         (id, server_id,
                 name,
                 disposition,          -- 0=Friendly 1=Neutral 2=Hostile 3=Unknown
                 -- Friendly = never attack, attacking incurs evil points / lawful penalty
                 -- Neutral  = attack if scripting+clearing, avoid if passing through
                 -- Hostile  = attack immediately
                 -- Unknown  = wait to be attacked, react, alert user
                 -- NOTE: live ANSI colour overrides DB disposition always:
                 --   Pink/Magenta=Hostile, Green/Grey=Neutral, White/Bold=Friendly
                 min_level, max_level,
                 notes,
                 source_id,            -- NULL if user-created
                 created_at, updated_at)

RoomMonster     (id, room_id, monster_id, spawn_chance)

Item            (id, server_id,
                 name,
                 weight_units,
                 value_copper,
                 is_collectible,
                 currency_value_copper,   -- NULL if not currency
                 currency_denomination,   -- 1=copper 2=silver 3=gold... NULL if not currency
                 notes,
                 source_id,              -- NULL if user-created
                 created_at, updated_at)

-- Paths and circuits
Path            (id, server_id,
                 name,
                 start_room_id,
                 end_room_id,          -- NULL for circuits
                 is_circuit,
                 created_at, updated_at)

PathStep        (id, path_id,
                 step_order,
                 command,              -- e.g. "north", "go skiff", "push button"
                 expected_room_id,     -- NULL = no verify
                 relearn,              -- overwrite fingerprint on next run
                 retry_step,           -- step_order to jump to on failure
                 retry_limit,          -- default 3
                 gold_required,        -- copper, NULL=no requirement
                 level_required,       -- NULL=no requirement
                 required_item_ids,    -- comma-separated item IDs, NULL=none
                 wait_ms,
                 is_stash_point,       -- evaluate stash here
                 is_search_point,      -- issue search command here
                 is_bank_point,        -- evaluate bank deposit here (standing in a bank)
                 is_rest_point,        -- rest/meditate before proceeding regardless of thresholds
                 created_at)

CharacterPath   (id, character_id, path_id,
                 override_active,
                 created_at, updated_at)

CharacterPathStep (id, character_path_id,
                   step_order, command, expected_room_id, relearn,
                   retry_step, retry_limit, gold_required, level_required,
                   required_item_ids, wait_ms,
                   is_stash_point, is_search_point, is_bank_point, is_rest_point)

-- Sought items
CharacterSoughtItem (id, character_id,
                     item_id,
                     min_quantity,
                     max_quantity,
                     collect_from_room,
                     collect_from_search)

-- Stash items
CharacterStashItem  (id, character_id,
                     item_id,
                     keep_quantity)

-- Attention panel
AttentionEvent  (id, character_id,
                 event_type,
                 description,
                 step_id,
                 resolved,
                 created_at)
```

---

## CMake Project Structure

```
MeagreMUD/
├── CMakeLists.txt
├── cmake/MeagreMUDVersion.cmake
├── meagrelib/
│   ├── CMakeLists.txt
│   └── src/
│       ├── protocol/
│       │   ├── Protocol.h
│       │   ├── FrameHeader.h
│       │   ├── Messages.h
│       │   └── StyledRun.h
│       ├── types/
│       │   └── MudTypes.h        # Line, StyledRun, CharacterStatus, Trigger,
│       │                         # CharacterConfig, SessionStats, PartyMember,
│       │                         # PartyMemberCapability
│       └── db/
│           ├── Schema.h
│           ├── DatabaseManager.h / .cpp
│           └── FingerprintBuilder.h / .cpp   # shared by daemon + scraper
├── daemon/
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp
│       ├── DaemonServer.h / .cpp
│       ├── GuiConnection.h / .cpp
│       ├── CharacterSession.h / .cpp
│       ├── CharacterRegistry.h / .cpp
│       ├── AnsiParser.h / .cpp
│       ├── AnsiStripper.h / .cpp
│       ├── ScriptEngine.h / .cpp
│       ├── PathEngine.h / .cpp
│       └── SessionLogger.h / .cpp
├── gui/
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp
│       ├── MainWindow.h / .cpp
│       ├── DaemonConnectionManager.h / .cpp
│       ├── TerminalWidget.h / .cpp
│       ├── CharacterPane.h / .cpp
│       ├── StatusLine.h / .cpp
│       ├── InputWidget.h / .cpp
│       ├── AttentionPanel.h / .cpp
│       ├── StatsWindow.h / .cpp
│       ├── Console.h / .cpp
│       ├── Settings.h / .cpp
│       ├── SettingsDialog.h / .cpp
│       └── PathEditor.h / .cpp
├── scraper/
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp
│       ├── BtrieveReader.h / .cpp      # raw Btrieve .dat file walker, no DLL
│       ├── NativeDbImporter.h / .cpp   # MajorMUD v1.11p field offset maps
│       └── WorldKnowledgeWriter.h / .cpp
└── test/
    ├── CMakeLists.txt
    └── networkTest/
```

---

## Daemon Implementation Structure

### Threading model
- **Main thread:** `DaemonServer`, `CharacterRegistry`, all `GuiConnection`s
- **Session thread N:** `CharacterSession`, `AnsiParser`, `AnsiStripper`, `ScriptEngine`, `PathEngine`, `SessionLogger`, `QTcpSocket` (MUD side)
- One `QThread` per `CharacterSession`
- `QTcpSocket` created in `CharacterSession::init()` — never in constructor
- Cross-thread signals use Qt automatic queued connection

### Component responsibilities

**`AnsiParser`** — SGR is server→client only, never from client.
Emits `runReady(StyledRun)` on escape completion. `\n` never triggers flush.

SGR codes handled: 0=reset, 1=bold, 30–37=fg normal, 39=fg default, 40–47=bg normal,
49=bg default, 90–97=fg bright, 100–107=bg bright.

Non-SGR: `ESC[2J`=screenCleared(), `ESC[H`=drop, `\r`=pendingCR flag.

**`AnsiStripper`** — emits `strippedText(QString)`. Internal buffering to `\n` for trigger granularity.

**`ScriptEngine`** — evaluates triggers, maintains tracked variables (HP, mana, exp, gold, encumbrance). Consumes `@meagre` telepath silently. Reloads triggers immediately on settings save. Also receives raw `StyledRun` stream for colour-based monster disposition detection.

**`PathEngine`** — manages three modes (Off/SemiAuto/Auto) and combat flag (Passive/Attack). Owns path history ring buffer for flee. Manages rest/meditate decision logic. Tracks session statistics.

**`SessionLogger`** — appends stripped text to log file.

### Signal/slot flow
```
MUD bytes → AnsiParser::onData()
    │ runReady(StyledRun)
    ├──► CharacterSession::onRunReady()
    │        │ runReady(charId, StyledRun) [queued]
    │        ▼ DaemonServer::broadcastStyledRun()
    │          → GuiConnection::sendFrame() [× N GUIs]
    │
    ├──► ScriptEngine::onRunReady()   ← colour inspection for monster disposition
    │
    └──► AnsiStripper::onRunReady()
             │ strippedText(QString)
             ├──► ScriptEngine::onStrippedText()
             ├──► PathEngine::onStrippedText()
             └──► SessionLogger::onStrippedText()
```

---

## AnsiParser State Machine

```
Normal: 0x1B→EscapeStart, \r→pendingCR, other→append to run
EscapeStart: '[' →CSI, other→drop ESC re-process
CSI: digits/';'→CSIParams, 'H'→drop, 'm'→SGR reset, '2'→CSIErase, other→drop
CSIParams: digits/';'→accumulate, 'm'→parse SGR emit run apply style, other→drop
CSIErase: 'J'→screenCleared(), other→drop
```

`pendingCR`: next `\n` → append `\r\n`; else → append `\r`, process byte.

---

## Three Operating Modes + Combat Flag

| Mode | Path Engine | Reactive Triggers | Human Input |
|---|---|---|---|
| Off | Inactive | Inactive | Full control |
| SemiAuto | Inactive | Active | Full control |
| Auto | Active | Active | At own risk |

**Combat flag:**
- **Passive:** Never initiate, never retaliate — keep moving. Absolute. Character takes hits and continues.
- **Attack:** Engage hostiles immediately. Engage neutrals if clearing room.

**Monster disposition from ANSI colour (live colour always overrides DB):**
- Pink/Magenta = Hostile → attack immediately if Attack mode
- Green/Grey = Neutral → attack if clearing, avoid if passing through
- White/Bold = Friendly → never attack (hard block regardless of mode)
- Unknown colour → wait to be attacked, react, log AttentionEvent

**Combat priority:** Hostiles first, then neutrals (if clearing), friendlies never, unknown never (react only).

---

## Path Engine State Machine

```
Idle → PreflightCheck → fail → PreflightFailed (→ Idle)
                      → pass → Running
Running:
    send flee_steps move commands in one burst (no step-by-step evaluation mid-flee)
    verify room fingerprint on arrival
    match → advance step
    unknown → AttentionEvent, advance anyway
    mismatch → jump retry_step; retry_limit exceeded → AttentionEvent → AwaitingHuman
    combat fires → Paused → (combat ends) → Running
    flee condition → send flee_steps commands rapidly → verify → rest if clear
    AbortPath → Idle
    circuit complete → loop
    point-to-point complete → Idle
```

**All laps including the first run with the configured combat flag.** If the user wants to traverse without combat on the first lap they set the combat flag to Passive — the engine has no special-case first-lap logic.

**Flee behaviour:**
- Send `flee_steps` move commands in rapid succession retracing path history (inverse directions)
- Stop early if safe room (`is_safe = true`) reached mid-flee
- On arrival: if hostile present → flee another `flee_steps`; if clear → rest/meditate
- Room verification still performed on arrival (not mid-flee)
- Once thresholds recovered → resume circuit from current position

**Inverse direction:** Stored explicitly in `RoomExit.inverse_direction`. If `NULL` (one-way exit) — skip that step during flee, use next available.

---

## Rest / Meditate Decision Logic

```
hp_deficit   = resume_threshold_hp_pct   - hp_current_pct   (0 if at/above threshold)
mana_deficit = resume_threshold_mana_pct - mana_current_pct (0 if at/above threshold)

if hp_deficit == 0 and mana_deficit == 0: stand, resume

if meditation_enabled and mana_deficit > hp_deficit: meditate
else: rest

-- Re-evaluate on each HP/mana update while stopped:
if meditation_enabled and mana_deficit > hp_deficit and currently resting:
    stand → meditate  (switch)
if (not meditation_enabled or hp_deficit >= mana_deficit) and currently meditating:
    stand → rest  (switch)
```

Resting and meditating are mutually exclusive. Switch = `stand\r\n` then new command.

**Rest point steps** (`is_rest_point = true`) trigger rest/meditate before proceeding regardless of current HP/mana thresholds.

---

## Statistics System

Pure in-memory per `CharacterSession`. No DB persistence. Lost on session end — intentional.

**Odometer resets:** Manual (user clicks reset), circuit start (automatic), new session.

**Tracked metrics:** Circuit count, current lap, gold gained, exp gained, monsters killed, hit/miss counts, times attacked/fled, time breakdown (fighting/resting-HP/resting-mana/resting-both/meditating/moving/idle), HP regen rate (active/rest), mana regen rate (active/rest/meditate).

**Regen rates:** Rolling averages computed from HP/mana delta snapshots bucketed by current engine state. Regen happens always — rest/meditate just increases the rate.

**Auto-suspend:** If `auto_suspend_exp_per_hour` configured and exp/hour drops below threshold after `auto_suspend_check_after_ms` — suspend session, emit Warning, add AttentionEvent.

**Stats window:** Non-modal always-on-top `QWidget`, one per character. Plain `paintEvent`, monospaced font, ASCII progress bars for time breakdown. `[Reset Odo]` button sends `0x47 StatsReset`.

---

## Inventory / Economy System

**Inventory tracking:** Engine reads full inventory on session start (`inventory` command, parse response). Tracks items gained/lost during play. Tracks total weight against `encumbrance_limit`.

**Currency collection priority:** Collect highest denomination first. When trading up: pick up higher denomination first, then drop lower. Never drop first.

**Gold collection:** If picking up a gold pile would exceed `encumbrance_limit` — skip it.

**Bank deposit calculation:**
```
deposit_amount = gold_carried
               - toll_cost_to_return    (sum of gold_required steps from here to circuit return)
               - cash_reserve_copper    (configurable per-character buffer)
```
If `deposit_amount <= 0` — skip deposit. Command: `deposit <amount>\r\n`.

**Stash points:** Any `PathStep` with `is_stash_point = true`. Engine evaluates stash condition when passing through. Items in `CharacterStashItem` dropped (keeping `keep_quantity`).

**Search points:** Any `PathStep` with `is_search_point = true`. Engine sends `search\r\n`. Items matching `CharacterSoughtItem` collected up to `max_quantity`.

**Sought items:** `CharacterSoughtItem` table. Min/max quantity thresholds. Pre-flight warns if below `min_quantity`. Light sources canonical example.

**Pre-flight item check:** Missing required items (`PathStep.required_item_ids`) are non-blocking warnings. Missing min-quantity sought items are also warnings. Both listed clearly to user.

**Auto-training:** Explicitly out of scope.

---

## Party System

**Party state is transient — not persisted to DB.** All party state held in-memory on leader's `CharacterSession`.

```cpp
struct PartyMemberCapability {
    Capability  capability;   // cure_blind, heal_single, heal_party, bless,
                              // boost_party, cure_poison, resurrect(future)
    QString     command;      // e.g. "cast heal\r\n"
    int         manaCost;
};

struct PartyMember {
    QString                      name;
    ClientType                   clientType;    // Unknown/MeagreMUD/MegaMUD/Manual
    int                          clientVersion;
    int                          hpCurrent, hpMax;
    int                          manaCurrent, manaMax;
    MemberStatus                 status;        // Active/KnockedOut/Separated/Unknown
    QList<PartyMemberCapability> capabilities;
    QDateTime                    lastSeen;
};
```

**Party state updated via:** Periodic `party` command poll (primary) + real-time server message parsing.

**Leader proceeds when:** Poll shows all members adequate HP/mana. Holds on KnockedOut until cleared in poll.

---

## @meagre Telepath Protocol

**Format:** `tell <name> @meagre <version> <command> [params]`
Version `1` current. Plain ASCII. Consumed silently by `ScriptEngine`.
Consumption pattern: `* tells you '@meagre # *'`

**Handshake:**
```
Leader → member:  tell <member> @meagre 1 hello
Member → leader:  tell <leader> @meagre 1 ok <version> caps:<bitmask>
```

Capability bitmask:
```
0x01  cure_blind
0x02  heal_single
0x04  heal_party
0x08  bless
0x10  boost_party
0x20  cure_poison
0x40  resurrect (future)
```

Leader requests full capability detail after handshake:
```
tell <member> @meagre 1 request caps_detail
member: tell <leader> @meagre 1 caps_detail <cap>:<command> [<cap>:<command> ...]
```

No retry on handshake. Timeout from `meagre_handshake_timeout_ms`. No reply → try MegaMUD `@status`. Still no reply → Manual.

**Leader → member commands:**

| Command | Meaning |
|---|---|
| `wait` | Hold position |
| `follow <leader>` | Re-follow after separation |
| `status` | Request status report |
| `disbanding` | Party disbanding |
| `request heal <name>` | Request cast heal on named member |
| `request cure_blind <name>` | Request cure blindness on named member |
| `request bless <name>` | Request bless on named member |
| `request boost_party` | Request party buff |

**Member → leader:**

| Command | Params | Meaning |
|---|---|---|
| `alert hp` | `<cur> <max>` | HP critical |
| `alert mana` | `<cur> <max>` | Mana critical |
| `alert separated` | — | Left party |
| `alert dead` | — | Died |
| `alert blind` | — | Needs cure blindness |
| `status reply` | `<hp_c> <hp_m> <mana_c> <mana_m>` | Status response |
| `confirm heal <name>` | — | Heal cast |
| `confirm cure_blind <name>` | — | Cure cast |
| `confirm bless <name>` | — | Bless cast |
| `confirm boost_party` | — | Buff cast |
| `decline heal <name>` | — | Insufficient mana / in combat |
| `decline cure_blind <name>` | — | Unavailable |

**Cure/heal coordination:** Leader selects first available capable member (not in combat, mana > resume threshold). Single request at a time — no double-cast possible.

**Heal trigger conditions:**
- Target HP below `request_heal_threshold_hp_pct`
- Healer mana above healer's own `resume_threshold_mana_pct`
- Healing during rest is valid — does not interrupt resting member

**Pre-rest sequence:**
1. Request party buffs/blessings
2. Request heals for critically low members
3. Rest/meditate
4. On resume — `post_combat_command` per character (sneak/hide etc.)

**MegaMUD members:** `@rest`/`@resume`/`@status`/`@comeback`. Rest timeout-bound.
**Manual members:** Visible party chat. `manual_response_timeout_ms` on leader's `Character`.

---

## Blindness Handling

**Three modes** (`blindness_mode` on `Character`):
- **0 = Wait:** Suspend movement, continue combat reactions, wait for vision clears message
- **1 = Ignore:** Keep moving. Prompt returning = move succeeded. `no exit` message = move failed. No fingerprint verification while blind.
- **2 = CureSpell:** Send `blindness_cure_command`. On failure fall back to Wait.

**Party blindness:** Blind member sends `alert blind` → leader coordinates cure via capability system.

---

## Knocked Down Handling

Engine detects knocked-down message → sets `KnockedDown` state → continues combat → suppresses all movement until stand-up message received. If fleeing when knocked down — waits until standing, then resumes flee.

---

## World Knowledge — NativeDbImporter

### Source format

MajorMUD v1.11p native Btrieve database files. Targeting latest version only — no multi-version support.

### Approach

`BtrieveReader` is a standalone raw file walker — reads `.dat` files directly as binary, no `WBTRV32.DLL`, no Windows dependency. Btrieve 6.x fixed-format records: file header at offset 0 gives record length and page size; data pages follow at page-size boundaries; each record preceded by a 1-byte usage flag.

Field offset maps derived from Nightmare Redux (`modMain.bas` → `Sub IntFieldMaps`) for v1.11p. All offset knowledge is private to `NativeDbImporter.cpp` behind `Offsets::Room`, `Offsets::Monster`, `Offsets::Item` namespaces.

### File inventory

| File | Contents |
|---|---|
| `w<CC>mp002.dat` | Rooms |
| `w<CC>knms2.dat` | Monsters |
| `w<CC>knit2.dat` | Items |
| `w<CC>race2.dat` | Races |
| `w<CC>clas2.dat` | Classes |

`<CC>` = BBS call letters (e.g. `WCC`), configured via `native_db_version` quirk.

### Interface

```cpp
class NativeDbImporter {
public:
    struct ImportResult {
        int roomsImported;
        int monstersImported;
        int itemsImported;
        int exitsImported;
        int spawnRecordsImported;
        QStringList warnings;
    };
    explicit NativeDbImporter(DatabaseManager &db, int serverId);
    ImportResult import(const QString &sourceDir);
private:
    void importRooms(const QString &path);
    void importMonsters(const QString &path);
    void importItems(const QString &path);
    void importExits(const QString &path);
    void importSpawns(const QString &path);
};
```

### Output

Import populates `seed.db` `Seeded_*` tables. All imported records have `is_verified = false`; `fingerprint` left NULL until confirmed by live play. `source_id` preserves original MajorMUD record ID. `is_verified` and `fingerprint` on existing records are never overwritten by re-import.

### Usage

```
meagre-scrape import --db <seed.db> --server-id <id> <source_dir>
```

---

## World Knowledge — seed.db Lifecycle

1. `meagre-scrape` runs offline against a MajorMUD installation directory, produces `seed.db`
2. `seed.db` ships read-only with MeagreMUD (installed to system data path)
3. On first run, `DatabaseManager` creates `meagremud.db`, populates `Seeded_*` tables from `seed.db`
4. On subsequent runs, `DatabaseManager` compares `SeedMeta.seed_version` to `Meta.applied_seed_version` — if different, drops and repopulates `Seeded_*` tables silently
5. User table records (`Room`, `Monster`, `Item` with `source_id` populated = promoted from seed, with `source_id` NULL = user-created) are never touched by seed reload
6. `seed.db` also serves as the shareable golden seed for community distribution

---

## GUI Implementation

### MainWindow
```
MainWindow
├── QMenuBar
│   └── (includes Path Editor launch item)
├── TiledArea (visible only when tiles exist)
├── QTabWidget (docked characters)
├── AttentionPanel (collapsible drawer, above tab bar)
├── QStatusBar (global)
└── DaemonConnectionManager
```

### AttentionPanel
Non-modal collapsible drawer. `⚠ N issues` badge when collapsed. Each entry: character, timestamp, type, description, Dismiss button. Dismiss sends `0x43 ClearAttention`.

### CharacterPane
`TerminalWidget` + `StatusLine` + `InputWidget`. Same instance reparented on dock/undock. Font/palette/macros resolved via global → BBS → character inheritance.

### TerminalWidget
Circular backbuffer, tile height snaps to whole lines, `\r` without `\n` overwrites open line, mouse-only scrollback, observer grayout overlay.

### StatusLine
Plain `paintEvent`. Shows: character name, BBS, connection status (colour coded), engine mode, combat flag (sword icon), current room name, script running indicator.

### InputWidget
`QLineEdit` + macro interception. `Return`/`Enter` sends text + `\r\n`. Locked during resync.

### StatsWindow
Non-modal always-on-top `QWidget`, one per character. Plain `paintEvent`, monospaced. ASCII progress bars for time breakdown including rest-HP/rest-mana/rest-both/meditate sub-buckets. Regen rates shown per state. Auto-suspend threshold shown if configured. `[Reset Odo]` button.

### Settings Dialog
Single `QDialog`, six tabs, OK/Cancel/Apply.
- **Tab 1:** Servers & Characters — flat list, full character config including rest thresholds, meditation, flee steps, blindness mode, post-combat command, bank config, encumbrance limit, auto-suspend
- **Tab 2:** Display — scope selector, font, dark ANSI default palette
- **Tab 3:** Key Macros — scope selector, key capture
- **Tab 4:** Triggers & Automation — trigger group tree, regular and party trigger edit, sought items, stash items
- **Tab 5:** Server Quirks — key/value table
- **Tab 6:** Daemon Connection — stored in `QSettings` (local INI), not SQLite

---

## Path/Circuit Editor

### Window

Non-modal `QWidget`, launched from menu. Single instance — re-focuses if already open. Resizable.

```
PathEditor
├── QSplitter (horizontal)
│   ├── Left pane — Path List
│   │   ├── QTreeWidget
│   │   │   ├── Circuits
│   │   │   │   └── "Inn to Crypt Circuit"
│   │   │   └── Point-to-Point
│   │   │       └── "Bank to Dungeon Entrance"
│   │   └── [New Circuit] [New Path] [Delete] [Rename]
│   │
│   └── Right pane — Path Detail
│       ├── Header bar
│       ├── Recording toolbar
│       └── QTabWidget
│           ├── Steps tab
│           └── Map tab
```

### Header bar

- Name (editable), Server (read-only), Start Room (room picker)
- End Room (room picker) — Point-to-Point only; hidden for circuits, replaced with read-only *"Loops back to start"* label
- **[Run Preflight]** button — triggers `0x42 PreflightRequest` for selected character; results appear inline on step rows

### Recording toolbar

```
[▶ Record]  [■ Stop]  [⏸ Pause]  |  Character: [dropdown]  |  [Insert Manual Step]
```

- During recording, every command the user types is appended as a step verbatim — no filtering or second-guessing
- `room_id` is included in each `RecordingStep` message; non-movement commands carry `room_id = 0x0000` and get no Expected Room filled in; movement commands that produce a `RoomIdentified` event carry the resolved `room_id` and auto-fill Expected Room
- `[Insert Manual Step]` is available when not recording, for building or editing paths manually without a live character
- Status note during recording: *"All commands are being recorded — type your lap normally."*
- Wait times, flags, and retry settings are always set manually after recording

### Steps tab

`QTableWidget` with columns:

```
# | Command | Expected Room | Flags | Wait(ms) | Notes
```

- **#** — step order, renumbers automatically on drag-reorder
- **Command** — editable inline; free text sent verbatim to MUD
- **Expected Room** — room name resolved from `source_id`; click to open Room Browser; blank = no verify; shows `[unverified]` if room is in DB but `is_verified = false`
- **Flags** — icon row, click to toggle:
  - 🔄 relearn — overwrite fingerprint on next run
  - 🏦 bank point — evaluate deposit here (we are in a bank)
  - 📦 stash point — evaluate stash/drop here
  - 🔍 search point — issue search command here
  - 💤 rest point — rest/meditate before proceeding regardless of thresholds
- **Wait(ms)** — inline spinbox, 0 = no wait
- **Notes** — free text, editor-only, not sent to daemon

Right-click context menu: Insert Above, Insert Below, Delete, Set Retry Target (picks step by number), Set Retry Limit.

Drag-and-drop row reorder — step numbers update live.

Preflight issues shown as ⚠ icon on affected rows; hover for detail. Clears on any path edit.

### Map tab

**Symbols:**
```
*          room
+          room with exits in 4+ cardinal directions
[*]        room on current path
[ ]        currently selected room (cursor)
 *         unverified room (dimmed)
|          north/south exit
-          east/west exit
/          northeast/southwest exit
\          northwest/southeast exit
^          up exit available
v          down exit available
^v         both up and down available
```

**Viewport:** Fixed ~21×21 cell grid. Re-centres on last path step when a step is appended. Scrollable for browsing.

**Z-level navigation:** Toolbar shows current level name with `[↑ Level Name]` / `[↓ Level Name]` buttons when up/down exits exist from the selected room. Clicking switches the view to that Z-level centred on the destination room.

**Click-to-build:** Rooms reachable via a direct exit from the last path step are clickable. Clicking appends the correct exit command as a new step and advances the path cursor. Non-reachable rooms render but do not respond to clicks. Non-movement steps (`go hole`, `push button`) are inserted via `[Insert Manual Step]` in the recording toolbar — the map re-evaluates reachability after insertion.

**Unverified rooms** render dimmed. Topology may be incomplete for unverified zones — the step list remains authoritative.

**Layout algorithm:** Recursive grid placement from start room at (0,0), exit directions as constraints, backtrack on collision. `[Tidy Layout]` button in map toolbar re-runs placement. Dense zones may produce overlaps — fall back to manual step entry.

### Room Browser

Small `QDialog`, searchable by name or zone. Shows `is_verified` status per room. Used by Expected Room column and right-click menu.

---

## Path Import/Export (CSV)

One file per path. Header block (comment lines prefixed `#`) carries path metadata. Rows are steps in order.

```csv
# MeagreMUD Path Export v1
# name: Inn to Crypt Circuit
# type: circuit
# start_room: 1/245
# exported_at: 2026-05-06
#
# step,command,expected_room,relearn,is_rest_point,is_stash_point,is_search_point,is_bank_point,wait_ms,retry_step,retry_limit,gold_required,level_required,notes
1,north,1/246,0,0,0,0,0,0,,3,,,
2,east,1/247,0,0,0,0,0,0,,3,,,
3,go hole,1/301,0,0,0,0,0,500,,3,,,
4,north,,0,0,1,0,0,0,,3,,,Search point - no verify
5,south,1/247,1,0,0,0,0,0,,3,,,Relearn on return
```

- `expected_room` uses `map/room` notation (`1/246`) — maps to `(map_number, room_number)` for lookup against `v_Room` on import
- Boolean flags are `0`/`1`
- Optional fields left empty rather than `0` to avoid ambiguity
- `notes` is last column — may contain commas, handled by standard CSV quoting

**Import behaviour:** Resolves `expected_room` via `v_Room`. Rooms not found → step imported with `expected_room_id = NULL`, warning logged. Import completes regardless of missing rooms. Summary shown after import: steps imported, rooms resolved, rooms not found. Always imports into user path tables.

**Export:** Right-click path in left pane → Export to CSV. Standard file save dialog.

**Import:** `[Import CSV]` button below path list. Preview dialog shows metadata and step count before committing. Name collision → prompt: Replace / Import as Copy / Cancel.

---

## GreaterMUD Notes

GreaterMUD is a MajorMUD fork. Its database format is assumed to be identical or near-identical to MajorMUD v1.11p for world data purposes. The GreaterMUD live server is the primary target but its database files are not available for scraping — MajorMUD's `seed.db` serves as the world knowledge seed (99% coverage). Divergences noted in comments in `NativeDbImporter.cpp` as discovered through live play and `relearn` verification.
