#pragma once

/**
 * @file Schema.h
 * @brief All SQL DDL for meagremud.db and seed.db as compile-time string constants.
 *
 * DatabaseManager uses Schema::ALL_CREATE_STATEMENTS[] to create the full
 * schema on first run. The array is null-terminated and applied in order.
 *
 * ## Schema versioning
 * @c db_schema_version is stored in the @c Meta table. Each integer version
 * maps to a migration script applied in sequence by DatabaseManager. Current
 * version: Schema::CURRENT_SCHEMA_VERSION.
 *
 * ## Dual-database architecture
 * | Database | Tables | Access |
 * |----------|--------|--------|
 * | seed.db | Seeded_Room, Seeded_Monster, etc. + SeedMeta | Read-only |
 * | meagremud.db | User tables + Seeded_* copies + Meta + views | Read-write |
 *
 * All world-knowledge queries use the @c v_* views that merge both layers.
 *
 * @see DatabaseManager
 */

// ---------------------------------------------------------------------------
// Schema.h
//
// All SQL DDL for meagremud.db and seed.db as string constants.
// DatabaseManager uses these to create tables and views on first run
// and to run migrations on upgrade.
//
// Schema versioning:
//   db_schema_version is stored in the Meta table of meagremud.db.
//   Each integer version maps to a SQL migration script applied in order.
//   Current schema version: 1
//
// Two-database architecture:
//   seed.db    — read-only, Seeded_* tables + SeedMeta
//   meagremud.db — read-write, user tables + Seeded_* copies + Meta + views
//
// View convention:
//   v_Room, v_Monster, v_Item, v_RoomExit, v_RoomMonster
//   User record wins over seeded record where source_id matches.
//   All application code reads through views; writes go to user tables only.
// ---------------------------------------------------------------------------

namespace Schema {

// ---------------------------------------------------------------------------
// Current version
// ---------------------------------------------------------------------------

static constexpr int CURRENT_SCHEMA_VERSION = 1;

// ---------------------------------------------------------------------------
// meagremud.db — Meta table
// ---------------------------------------------------------------------------

static constexpr const char *CREATE_META = R"SQL(
CREATE TABLE IF NOT EXISTS Meta (
    key   TEXT PRIMARY KEY NOT NULL,
    value TEXT NOT NULL
);
)SQL";

// ---------------------------------------------------------------------------
// meagremud.db — Core hierarchy
// ---------------------------------------------------------------------------

static constexpr const char *CREATE_MUD_SERVER = R"SQL(
CREATE TABLE IF NOT EXISTS MudServer (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT    NOT NULL,
    host            TEXT    NOT NULL,
    port            INTEGER NOT NULL DEFAULT 23,
    mud_type        INTEGER NOT NULL DEFAULT 0,  -- 0=GreaterMUD 1=MajorMUD
    backbuffer_lines INTEGER NOT NULL DEFAULT 1000,
    created_at      DATETIME NOT NULL DEFAULT (datetime('now')),
    updated_at      DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

static constexpr const char *CREATE_MUD_SERVER_QUIRK = R"SQL(
-- Default quirk keys:
--   meagre_handshake_timeout_ms   (default 5000)
--   rest_timeout_ms               (MegaMUD rest timeout)
--   native_db_version             (default '1.11p') -- NativeDbImporter field map
CREATE TABLE IF NOT EXISTS MudServerQuirk (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    server_id   INTEGER NOT NULL REFERENCES MudServer(id) ON DELETE CASCADE,
    quirk_key   TEXT    NOT NULL,
    quirk_value TEXT    NOT NULL,
    UNIQUE (server_id, quirk_key)
);
)SQL";

static constexpr const char *CREATE_USER_LOGIN = R"SQL(
CREATE TABLE IF NOT EXISTS UserLogin (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    server_id   INTEGER NOT NULL REFERENCES MudServer(id) ON DELETE CASCADE,
    username    TEXT    NOT NULL,
    password    TEXT    NOT NULL,
    created_at  DATETIME NOT NULL DEFAULT (datetime('now')),
    updated_at  DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

static constexpr const char *CREATE_MUD_INSTANCE = R"SQL(
CREATE TABLE IF NOT EXISTS MudInstance (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    login_id            INTEGER NOT NULL REFERENCES UserLogin(id) ON DELETE CASCADE,
    name                TEXT    NOT NULL,
    instance_selector   TEXT,   -- stimulus/response pattern for world selection
    created_at          DATETIME NOT NULL DEFAULT (datetime('now')),
    updated_at          DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

static constexpr const char *CREATE_CHARACTER = R"SQL(
CREATE TABLE IF NOT EXISTS Character (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    instance_id     INTEGER NOT NULL REFERENCES MudInstance(id) ON DELETE CASCADE,
    name            TEXT    NOT NULL,
    password        TEXT    NOT NULL,
    auto_start      INTEGER NOT NULL DEFAULT 0,  -- BOOLEAN
    auto_reconnect  INTEGER NOT NULL DEFAULT 1,  -- BOOLEAN

    backbuffer_lines_override   INTEGER,  -- NULL = inherit from server
    manual_response_timeout_ms  INTEGER NOT NULL DEFAULT 30000,

    -- Rest / meditate
    rest_threshold_hp_pct       INTEGER NOT NULL DEFAULT 50,
    rest_threshold_mana_pct     INTEGER NOT NULL DEFAULT 30,
    resume_threshold_hp_pct     INTEGER NOT NULL DEFAULT 90,
    resume_threshold_mana_pct   INTEGER NOT NULL DEFAULT 80,
    meditation_enabled          INTEGER NOT NULL DEFAULT 0,  -- BOOLEAN

    -- Party
    request_heal_threshold_hp_pct INTEGER NOT NULL DEFAULT 60,

    -- Flee
    flee_steps                  INTEGER NOT NULL DEFAULT 3,

    -- Encumbrance
    encumbrance_limit           INTEGER NOT NULL DEFAULT 0, -- 0=Unencumbered 1=Light 2=Medium 3=Heavy
    gold_collection_limit       INTEGER NOT NULL DEFAULT 0, -- 0=always collect 1=stop at encumbrance_limit

    -- Banking
    bank_threshold_copper       INTEGER,  -- NULL = never auto-bank
    bank_room_id                INTEGER REFERENCES Room(id),

    -- Post-combat
    post_combat_command         TEXT,     -- NULL = none
    post_combat_hp_pct          INTEGER,  -- NULL = always execute

    -- Blindness
    blindness_mode              INTEGER NOT NULL DEFAULT 0, -- 0=Wait 1=Ignore 2=CureSpell
    blindness_cure_command      TEXT,     -- NULL if mode != 2

    -- Auto-suspend
    auto_suspend_exp_per_hour   INTEGER,  -- NULL = disabled
    auto_suspend_check_after_ms INTEGER NOT NULL DEFAULT 1800000,

    created_at  DATETIME NOT NULL DEFAULT (datetime('now')),
    updated_at  DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

static constexpr const char *CREATE_LOGIN_SEQUENCE = R"SQL(
CREATE TABLE IF NOT EXISTS LoginSequence (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    character_id    INTEGER NOT NULL REFERENCES Character(id) ON DELETE CASCADE,
    step_order      INTEGER NOT NULL,
    stimulus        TEXT    NOT NULL,
    response        TEXT    NOT NULL,
    is_template     INTEGER NOT NULL DEFAULT 0,  -- BOOLEAN
    UNIQUE (character_id, step_order)
);
)SQL";

static constexpr const char *CREATE_SESSION_LOG = R"SQL(
CREATE TABLE IF NOT EXISTS SessionLog (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    character_id    INTEGER NOT NULL REFERENCES Character(id) ON DELETE CASCADE,
    started_at      DATETIME NOT NULL,
    ended_at        DATETIME,
    log_path        TEXT
);
)SQL";

// ---------------------------------------------------------------------------
// Display / input settings (three-level inheritance: global → MudServer → Character)
// ---------------------------------------------------------------------------

static constexpr const char *CREATE_DISPLAY_SETTINGS = R"SQL(
CREATE TABLE IF NOT EXISTS DisplaySettings (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    owner_type  INTEGER NOT NULL,  -- 0=global 1=MudServer 2=Character
    owner_id    INTEGER NOT NULL,  -- 0 for global
    setting_key TEXT    NOT NULL,
    setting_value TEXT  NOT NULL,
    UNIQUE (owner_type, owner_id, setting_key)
);
)SQL";

static constexpr const char *CREATE_KEY_MACRO = R"SQL(
CREATE TABLE IF NOT EXISTS KeyMacro (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    owner_type  INTEGER NOT NULL,
    owner_id    INTEGER NOT NULL,
    key_code    INTEGER NOT NULL,
    modifiers   INTEGER NOT NULL DEFAULT 0,
    sends_text  TEXT    NOT NULL
);
)SQL";

// ---------------------------------------------------------------------------
// Scripting
// ---------------------------------------------------------------------------

static constexpr const char *CREATE_TRIGGER_GROUP = R"SQL(
CREATE TABLE IF NOT EXISTS TriggerGroup (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    character_id    INTEGER NOT NULL REFERENCES Character(id) ON DELETE CASCADE,
    name            TEXT    NOT NULL,
    enabled         INTEGER NOT NULL DEFAULT 1  -- BOOLEAN
);
)SQL";

static constexpr const char *CREATE_TRIGGER = R"SQL(
CREATE TABLE IF NOT EXISTS Trigger (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    character_id        INTEGER NOT NULL REFERENCES Character(id) ON DELETE CASCADE,
    group_id            INTEGER REFERENCES TriggerGroup(id),
    pattern             TEXT    NOT NULL,  -- * = any text, # = any number
    threshold_capture   INTEGER,           -- which #N to compare, NULL=none
    threshold_op        TEXT,              -- '<' or '>'
    threshold_value     REAL,
    response            TEXT    NOT NULL,
    enable_group_id     INTEGER REFERENCES TriggerGroup(id),
    disable_group_id    INTEGER REFERENCES TriggerGroup(id),
    priority            INTEGER NOT NULL DEFAULT 0,
    enabled             INTEGER NOT NULL DEFAULT 1  -- BOOLEAN
);
)SQL";

static constexpr const char *CREATE_PARTY_TRIGGER = R"SQL(
CREATE TABLE IF NOT EXISTS PartyTrigger (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    character_id        INTEGER NOT NULL REFERENCES Character(id) ON DELETE CASCADE,
    member_status       INTEGER NOT NULL,  -- 0=KnockedOut 1=Separated 2=Dead
    client_type_filter  INTEGER NOT NULL DEFAULT 0, -- 0=Any 1=MeagreMUD 2=MegaMUD 3=Manual
    response            TEXT    NOT NULL,
    enabled             INTEGER NOT NULL DEFAULT 1
);
)SQL";

// ---------------------------------------------------------------------------
// World knowledge — user layer
//
// source_id NULL  = user-created record (never touched by seed reload)
// source_id set   = promoted from seed (user has customised this record)
//
// is_verified and fingerprint on Room are never overwritten by seed reload —
// they are owned by live play via the daemon's relearn mechanism.
// ---------------------------------------------------------------------------

static constexpr const char *CREATE_ROOM = R"SQL(
CREATE TABLE IF NOT EXISTS Room (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    server_id       INTEGER NOT NULL REFERENCES MudServer(id) ON DELETE CASCADE,
    name            TEXT    NOT NULL,
    fingerprint     TEXT,   -- NULL until confirmed by live play
                            -- NOT overwritten by seed reload
    zone            TEXT,
    is_verified     INTEGER NOT NULL DEFAULT 0,  -- BOOLEAN, NOT overwritten by seed reload
    is_safe         INTEGER NOT NULL DEFAULT 0,  -- BOOLEAN, monsters cannot enter
    source_id       INTEGER,  -- original MajorMUD room ID; NULL if user-created
    created_at      DATETIME NOT NULL DEFAULT (datetime('now')),
    updated_at      DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

static constexpr const char *CREATE_ROOM_EXIT = R"SQL(
CREATE TABLE IF NOT EXISTS RoomExit (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    room_id             INTEGER NOT NULL REFERENCES Room(id) ON DELETE CASCADE,
    direction           TEXT    NOT NULL,  -- e.g. "north", "go skiff"
    inverse_direction   TEXT,              -- NULL if one-way/unknown
    destination_room_id INTEGER REFERENCES Room(id),
    is_hidden           INTEGER NOT NULL DEFAULT 0,
    created_at          DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

static constexpr const char *CREATE_MONSTER = R"SQL(
CREATE TABLE IF NOT EXISTS Monster (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    server_id   INTEGER NOT NULL REFERENCES MudServer(id) ON DELETE CASCADE,
    name        TEXT    NOT NULL,
    disposition INTEGER NOT NULL DEFAULT 3, -- 0=Friendly 1=Neutral 2=Hostile 3=Unknown
    min_level   INTEGER,
    max_level   INTEGER,
    notes       TEXT,
    source_id   INTEGER,  -- NULL if user-created
    created_at  DATETIME NOT NULL DEFAULT (datetime('now')),
    updated_at  DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

static constexpr const char *CREATE_ROOM_MONSTER = R"SQL(
CREATE TABLE IF NOT EXISTS RoomMonster (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    room_id         INTEGER NOT NULL REFERENCES Room(id) ON DELETE CASCADE,
    monster_id      INTEGER NOT NULL REFERENCES Monster(id) ON DELETE CASCADE,
    spawn_chance    INTEGER NOT NULL DEFAULT 100
);
)SQL";

static constexpr const char *CREATE_ITEM = R"SQL(
CREATE TABLE IF NOT EXISTS Item (
    id                      INTEGER PRIMARY KEY AUTOINCREMENT,
    server_id               INTEGER NOT NULL REFERENCES MudServer(id) ON DELETE CASCADE,
    name                    TEXT    NOT NULL,
    weight_units            INTEGER NOT NULL DEFAULT 0,
    value_copper            INTEGER NOT NULL DEFAULT 0,
    is_collectible          INTEGER NOT NULL DEFAULT 1,
    currency_value_copper   INTEGER,  -- NULL if not currency
    currency_denomination   INTEGER,  -- 1=copper 2=silver 3=gold; NULL if not currency
    notes                   TEXT,
    source_id               INTEGER,  -- NULL if user-created
    created_at              DATETIME NOT NULL DEFAULT (datetime('now')),
    updated_at              DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

// ---------------------------------------------------------------------------
// Seeded world knowledge tables (mirrored structure, freely overwritten)
// ---------------------------------------------------------------------------

static constexpr const char *CREATE_SEEDED_ROOM = R"SQL(
CREATE TABLE IF NOT EXISTS Seeded_Room (
    id          INTEGER PRIMARY KEY,
    server_id   INTEGER NOT NULL,
    name        TEXT    NOT NULL,
    fingerprint TEXT,
    zone        TEXT,
    is_verified INTEGER NOT NULL DEFAULT 0,
    is_safe     INTEGER NOT NULL DEFAULT 0,
    source_id   INTEGER NOT NULL,
    created_at  DATETIME NOT NULL DEFAULT (datetime('now')),
    updated_at  DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

static constexpr const char *CREATE_SEEDED_ROOM_EXIT = R"SQL(
CREATE TABLE IF NOT EXISTS Seeded_RoomExit (
    id                  INTEGER PRIMARY KEY,
    room_id             INTEGER NOT NULL,
    direction           TEXT    NOT NULL,
    inverse_direction   TEXT,
    destination_room_id INTEGER,
    is_hidden           INTEGER NOT NULL DEFAULT 0,
    created_at          DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

static constexpr const char *CREATE_SEEDED_MONSTER = R"SQL(
CREATE TABLE IF NOT EXISTS Seeded_Monster (
    id          INTEGER PRIMARY KEY,
    server_id   INTEGER NOT NULL,
    name        TEXT    NOT NULL,
    disposition INTEGER NOT NULL DEFAULT 3,
    min_level   INTEGER,
    max_level   INTEGER,
    notes       TEXT,
    source_id   INTEGER NOT NULL,
    created_at  DATETIME NOT NULL DEFAULT (datetime('now')),
    updated_at  DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

static constexpr const char *CREATE_SEEDED_ROOM_MONSTER = R"SQL(
CREATE TABLE IF NOT EXISTS Seeded_RoomMonster (
    id              INTEGER PRIMARY KEY,
    room_id         INTEGER NOT NULL,
    monster_id      INTEGER NOT NULL,
    spawn_chance    INTEGER NOT NULL DEFAULT 100
);
)SQL";

static constexpr const char *CREATE_SEEDED_ITEM = R"SQL(
CREATE TABLE IF NOT EXISTS Seeded_Item (
    id                      INTEGER PRIMARY KEY,
    server_id               INTEGER NOT NULL,
    name                    TEXT    NOT NULL,
    weight_units            INTEGER NOT NULL DEFAULT 0,
    value_copper            INTEGER NOT NULL DEFAULT 0,
    is_collectible          INTEGER NOT NULL DEFAULT 1,
    currency_value_copper   INTEGER,
    currency_denomination   INTEGER,
    notes                   TEXT,
    source_id               INTEGER NOT NULL,
    created_at              DATETIME NOT NULL DEFAULT (datetime('now')),
    updated_at              DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

// ---------------------------------------------------------------------------
// Seed metadata (stored in meagremud.db after reload)
// ---------------------------------------------------------------------------

static constexpr const char *CREATE_SEED_META = R"SQL(
CREATE TABLE IF NOT EXISTS SeedMeta (
    seed_version    TEXT    NOT NULL,
    generated_at    DATETIME NOT NULL,
    source_version  TEXT    NOT NULL
);
)SQL";

// ---------------------------------------------------------------------------
// Views — merge seeded and user layers
//
// User record wins over seeded record where source_id matches.
// user_owned = 1 for user records, 0 for seeded fallbacks.
// ---------------------------------------------------------------------------

static constexpr const char *CREATE_VIEW_ROOM = R"SQL(
CREATE VIEW IF NOT EXISTS v_Room AS
    SELECT *, 0 AS user_owned FROM Seeded_Room
    WHERE (server_id, source_id) NOT IN (
        SELECT server_id, source_id FROM Room
        WHERE source_id IS NOT NULL
    )
    UNION ALL
    SELECT *, 1 AS user_owned FROM Room;
)SQL";

static constexpr const char *CREATE_VIEW_MONSTER = R"SQL(
CREATE VIEW IF NOT EXISTS v_Monster AS
    SELECT *, 0 AS user_owned FROM Seeded_Monster
    WHERE (server_id, source_id) NOT IN (
        SELECT server_id, source_id FROM Monster
        WHERE source_id IS NOT NULL
    )
    UNION ALL
    SELECT *, 1 AS user_owned FROM Monster;
)SQL";

static constexpr const char *CREATE_VIEW_ITEM = R"SQL(
CREATE VIEW IF NOT EXISTS v_Item AS
    SELECT *, 0 AS user_owned FROM Seeded_Item
    WHERE (server_id, source_id) NOT IN (
        SELECT server_id, source_id FROM Item
        WHERE source_id IS NOT NULL
    )
    UNION ALL
    SELECT *, 1 AS user_owned FROM Item;
)SQL";

static constexpr const char *CREATE_VIEW_ROOM_EXIT = R"SQL(
CREATE VIEW IF NOT EXISTS v_RoomExit AS
    SELECT *, 0 AS user_owned FROM Seeded_RoomExit
    WHERE room_id NOT IN (
        SELECT id FROM Room WHERE source_id IS NOT NULL
    )
    UNION ALL
    SELECT *, 1 AS user_owned FROM RoomExit;
)SQL";

static constexpr const char *CREATE_VIEW_ROOM_MONSTER = R"SQL(
CREATE VIEW IF NOT EXISTS v_RoomMonster AS
    SELECT *, 0 AS user_owned FROM Seeded_RoomMonster
    WHERE room_id NOT IN (
        SELECT id FROM Room WHERE source_id IS NOT NULL
    )
    UNION ALL
    SELECT *, 1 AS user_owned FROM RoomMonster;
)SQL";

// ---------------------------------------------------------------------------
// Paths and circuits
// ---------------------------------------------------------------------------

static constexpr const char *CREATE_PATH = R"SQL(
CREATE TABLE IF NOT EXISTS Path (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    server_id       INTEGER NOT NULL REFERENCES MudServer(id) ON DELETE CASCADE,
    name            TEXT    NOT NULL,
    start_room_id   INTEGER REFERENCES Room(id),
    end_room_id     INTEGER REFERENCES Room(id),  -- NULL for circuits
    is_circuit      INTEGER NOT NULL DEFAULT 0,
    created_at      DATETIME NOT NULL DEFAULT (datetime('now')),
    updated_at      DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

static constexpr const char *CREATE_PATH_STEP = R"SQL(
CREATE TABLE IF NOT EXISTS PathStep (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    path_id             INTEGER NOT NULL REFERENCES Path(id) ON DELETE CASCADE,
    step_order          INTEGER NOT NULL,
    command             TEXT    NOT NULL,
    expected_room_id    INTEGER REFERENCES Room(id),
    relearn             INTEGER NOT NULL DEFAULT 0,
    retry_step          INTEGER,  -- step_order to jump to on failure
    retry_limit         INTEGER NOT NULL DEFAULT 3,
    gold_required       INTEGER,  -- copper, NULL=no requirement
    level_required      INTEGER,
    required_item_ids   TEXT,     -- comma-separated item IDs, NULL=none
    wait_ms             INTEGER NOT NULL DEFAULT 0,
    is_stash_point      INTEGER NOT NULL DEFAULT 0,
    is_search_point     INTEGER NOT NULL DEFAULT 0,
    is_bank_point       INTEGER NOT NULL DEFAULT 0,
    is_rest_point       INTEGER NOT NULL DEFAULT 0,
    created_at          DATETIME NOT NULL DEFAULT (datetime('now')),
    UNIQUE (path_id, step_order)
);
)SQL";

static constexpr const char *CREATE_CHARACTER_PATH = R"SQL(
CREATE TABLE IF NOT EXISTS CharacterPath (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    character_id    INTEGER NOT NULL REFERENCES Character(id) ON DELETE CASCADE,
    path_id         INTEGER NOT NULL REFERENCES Path(id) ON DELETE CASCADE,
    override_active INTEGER NOT NULL DEFAULT 0,
    created_at      DATETIME NOT NULL DEFAULT (datetime('now')),
    updated_at      DATETIME NOT NULL DEFAULT (datetime('now')),
    UNIQUE (character_id, path_id)
);
)SQL";

static constexpr const char *CREATE_CHARACTER_PATH_STEP = R"SQL(
CREATE TABLE IF NOT EXISTS CharacterPathStep (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    character_path_id   INTEGER NOT NULL REFERENCES CharacterPath(id) ON DELETE CASCADE,
    step_order          INTEGER NOT NULL,
    command             TEXT    NOT NULL,
    expected_room_id    INTEGER REFERENCES Room(id),
    relearn             INTEGER NOT NULL DEFAULT 0,
    retry_step          INTEGER,
    retry_limit         INTEGER NOT NULL DEFAULT 3,
    gold_required       INTEGER,
    level_required      INTEGER,
    required_item_ids   TEXT,
    wait_ms             INTEGER NOT NULL DEFAULT 0,
    is_stash_point      INTEGER NOT NULL DEFAULT 0,
    is_search_point     INTEGER NOT NULL DEFAULT 0,
    is_bank_point       INTEGER NOT NULL DEFAULT 0,
    is_rest_point       INTEGER NOT NULL DEFAULT 0,
    UNIQUE (character_path_id, step_order)
);
)SQL";

// ---------------------------------------------------------------------------
// Inventory management
// ---------------------------------------------------------------------------

static constexpr const char *CREATE_CHARACTER_SOUGHT_ITEM = R"SQL(
CREATE TABLE IF NOT EXISTS CharacterSoughtItem (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    character_id        INTEGER NOT NULL REFERENCES Character(id) ON DELETE CASCADE,
    item_id             INTEGER NOT NULL,
    min_quantity        INTEGER NOT NULL DEFAULT 1,
    max_quantity        INTEGER NOT NULL DEFAULT 1,
    collect_from_room   INTEGER NOT NULL DEFAULT 1,
    collect_from_search INTEGER NOT NULL DEFAULT 1,
    UNIQUE (character_id, item_id)
);
)SQL";

static constexpr const char *CREATE_CHARACTER_STASH_ITEM = R"SQL(
CREATE TABLE IF NOT EXISTS CharacterStashItem (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    character_id    INTEGER NOT NULL REFERENCES Character(id) ON DELETE CASCADE,
    item_id         INTEGER NOT NULL,
    keep_quantity   INTEGER NOT NULL DEFAULT 0,
    UNIQUE (character_id, item_id)
);
)SQL";

// ---------------------------------------------------------------------------
// Attention events (persisted across sessions)
// ---------------------------------------------------------------------------

static constexpr const char *CREATE_ATTENTION_EVENT = R"SQL(
CREATE TABLE IF NOT EXISTS AttentionEvent (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    character_id    INTEGER NOT NULL REFERENCES Character(id) ON DELETE CASCADE,
    event_type      INTEGER NOT NULL,
    description     TEXT    NOT NULL,
    step_id         INTEGER REFERENCES PathStep(id),
    resolved        INTEGER NOT NULL DEFAULT 0,
    created_at      DATETIME NOT NULL DEFAULT (datetime('now'))
);
)SQL";

// ---------------------------------------------------------------------------
// Ordered list of all CREATE statements for meagremud.db
// DatabaseManager applies these in sequence on first run.
// ---------------------------------------------------------------------------

static constexpr const char *ALL_CREATE_STATEMENTS[] = {
    CREATE_META,
    CREATE_MUD_SERVER,
    CREATE_MUD_SERVER_QUIRK,
    CREATE_USER_LOGIN,
    CREATE_MUD_INSTANCE,
    CREATE_CHARACTER,
    CREATE_LOGIN_SEQUENCE,
    CREATE_SESSION_LOG,
    CREATE_DISPLAY_SETTINGS,
    CREATE_KEY_MACRO,
    CREATE_TRIGGER_GROUP,
    CREATE_TRIGGER,
    CREATE_PARTY_TRIGGER,
    CREATE_ROOM,
    CREATE_ROOM_EXIT,
    CREATE_MONSTER,
    CREATE_ROOM_MONSTER,
    CREATE_ITEM,
    CREATE_SEEDED_ROOM,
    CREATE_SEEDED_ROOM_EXIT,
    CREATE_SEEDED_MONSTER,
    CREATE_SEEDED_ROOM_MONSTER,
    CREATE_SEEDED_ITEM,
    CREATE_SEED_META,
    CREATE_VIEW_ROOM,
    CREATE_VIEW_MONSTER,
    CREATE_VIEW_ITEM,
    CREATE_VIEW_ROOM_EXIT,
    CREATE_VIEW_ROOM_MONSTER,
    CREATE_PATH,
    CREATE_PATH_STEP,
    CREATE_CHARACTER_PATH,
    CREATE_CHARACTER_PATH_STEP,
    CREATE_CHARACTER_SOUGHT_ITEM,
    CREATE_CHARACTER_STASH_ITEM,
    CREATE_ATTENTION_EVENT,
    nullptr  // sentinel
};

} // namespace Schema
