\mainpage MeagreMUD

**MeagreMUD** is an open source, multiplatform MajorMUD / GreaterMUD client written in C++17 with Qt6 and CMake. Licensed under GPL-3.0.

---

## Architecture Overview

The full architecture design document is in `docs/MeagreMUD-Architecture.md`. Key design decisions summarised here:

### Component structure

| Component | Binary | Purpose |
|-----------|--------|---------|
| `meagrelib` | static library | Shared protocol types, database schema, DatabaseManager |
| `meagre-daemon` | console executable | Session manager — runs on the embedded target, persists when GUI disconnects |
| `meagre-gui` | GUI executable | Qt6 display and input frontend |
| `meagre-scrape` | console executable | Offline MajorMUD world knowledge importer |

### Core design principles

- **No newline dependency.** The daemon never uses `\n` as a flush or segmentation trigger. MUD prompts sit on open lines indefinitely; any design that waits for `\n` stalls the display at the worst moment. `AnsiParser` flushes a `StyledRun` on escape-sequence completion only.
- **ANSI parsing in the daemon.** The GUI receives pre-parsed `StyledRun` data only — never raw escape codes.
- **Daemon/GUI split.** The daemon manages all character sessions and persists when the GUI disconnects. The GUI reconnects and resyncs via a binary TCP protocol.
- **DatabaseManager is the sole class aware of the dual-table structure.** All application code reads world knowledge through `v_Room`, `v_Monster`, `v_Item`, `v_RoomExit`, `v_RoomMonster` views.

### Threading model (daemon)

- **Main thread:** `DaemonServer`, `CharacterRegistry`, all `GuiConnection` instances.
- **Session thread N:** `CharacterSession`, `AnsiParser`, `AnsiStripper`, `ScriptEngine`, `PathEngine`, `SessionLogger`, `QTcpSocket` (MUD side). One `QThread` per character.
- Cross-thread signals use Qt automatic queued connection.

### GUI connection states

```
Disconnected → Connecting → Syncing → Connected
                               ↑
                    (re-enters on ProtocolError)
```

### Data hierarchy

```
MudServer → UserLogin → MudInstance → Character
```

---

## Module Index

- \ref meagrelib_group "meagrelib — Shared library"
- \ref daemon_group "daemon — Session manager"
- \ref gui_group "gui — Frontend"

---

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Requires Qt6 (Core, Network, Sql, Widgets) and CMake 3.22+.
Graphviz (`dot`) is optional but recommended for call graphs in the generated documentation.

To generate documentation:

```bash
doxygen docs/Doxyfile
# Output: docs/doxygen/html/index.html
```
