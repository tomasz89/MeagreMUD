#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariant>

// ---------------------------------------------------------------------------
// DatabaseManager
//
// Owns both database connections:
//   "meagre_main" — meagremud.db (read-write, user data + seeded copies)
//   "meagre_seed" — seed.db      (read-only, seeded world knowledge)
//
// On open():
//   1. Opens meagremud.db, creates schema if new (ALL_CREATE_STATEMENTS)
//   2. Runs pending schema migrations (db_schema_version in Meta)
//   3. Opens seed.db if present
//   4. Compares SeedMeta.seed_version — if different, reloads Seeded_* tables
//
// All application code reads world knowledge through v_Room, v_Monster,
// v_Item, v_RoomExit, v_RoomMonster views. Writes go to user tables only.
// DatabaseManager is the sole class that knows about the dual-table structure.
//
// Thread safety:
//   DatabaseManager is NOT thread-safe. It must be used from a single thread.
//   In the daemon this is the main thread. CharacterSessions on worker threads
//   do not use DatabaseManager directly — they receive configuration via
//   signals from the main thread.
//
// Connection names are constants to avoid typos at call sites.
// ---------------------------------------------------------------------------

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    static constexpr const char *MAIN_CONNECTION = "meagre_main";
    static constexpr const char *SEED_CONNECTION = "meagre_seed";

    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager() override;

    // Open both databases. seedDbPath may be empty if no seed is available.
    // Returns true if meagremud.db opened and schema is up to date.
    bool open(const QString &mainDbPath, const QString &seedDbPath = QString());

    void close();

    bool isOpen() const;

    // Returns the main (read-write) database connection.
    // Use this for all queries. Never cache across calls — connections can
    // be invalidated on close().
    QSqlDatabase mainDb() const;

    // Execute a single SQL statement on the main DB.
    // Returns true on success. Logs error on failure.
    bool exec(const QString &sql);

    // Execute a parameterised query on the main DB.
    // Binds values positionally (:1, :2, …) from the provided list.
    bool exec(const QString &sql, const QVariantList &bindings);

    // Read a value from the Meta table. Returns defaultValue if not found.
    QString metaValue(const QString &key,
                      const QString &defaultValue = QString()) const;

    // Write a value to the Meta table.
    bool setMetaValue(const QString &key, const QString &value);

    // Current applied schema version.
    int schemaVersion() const;

    // Whether seed.db was successfully opened and applied.
    bool seedLoaded() const { return m_seedLoaded; }
    QString appliedSeedVersion() const;

signals:
    // Emitted after the Seeded_* tables have been reloaded from seed.db.
    void seedReloaded(const QString &newVersion);

private:
    bool openMainDb(const QString &path);
    bool openSeedDb(const QString &path);
    bool createSchema();
    bool runMigrations();
    bool checkAndReloadSeed();
    bool reloadSeedTables();
    void copySeedToMain();

    static bool execOnDb(QSqlDatabase &db, const QString &sql);

    bool    m_open       = false;
    bool    m_seedLoaded = false;
};
