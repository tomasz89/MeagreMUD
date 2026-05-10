#pragma once

/**
 * @file DatabaseManager.h
 * @brief Dual-database manager for meagremud.db and seed.db.
 */


#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariant>

/**
 * @file DatabaseManager.h
 * @brief Owns and manages both SQLite database connections for MeagreMUD.
 */

/**
 * @brief Central database access point for the daemon and scraper.
 *
 * Owns two named QSqlDatabase connections:
 * - @c "meagre_main"  -  @c meagremud.db, read-write. Contains user data,
 *   Seeded_* copies, and the v_* views.
 * - @c "meagre_seed"  -  @c seed.db, read-only. Contains seeded world knowledge
 *   as shipped with MeagreMUD.
 *
 * On open():
 * -# Opens @c meagremud.db and creates the full schema if the database is new.
 * -# Runs any pending schema migrations based on @c db_schema_version in Meta.
 * -# Opens @c seed.db if the path is provided and the file exists.
 * -# Compares @c SeedMeta.seed_version against @c Meta.applied_seed_version;
 *    if they differ, silently reloads all @c Seeded_* tables from @c seed.db.
 *
 * @par Thread safety
 * DatabaseManager is @b not thread-safe. It must be used exclusively from
 * the main thread. CharacterSession worker threads receive configuration via
 * queued signals rather than calling DatabaseManager directly.
 *
 * @par View convention
 * All application code reads world knowledge through the @c v_Room,
 * @c v_Monster, @c v_Item, @c v_RoomExit, and @c v_RoomMonster views.
 * Writes go to user-layer tables only. DatabaseManager is the sole class
 * aware of the dual-table structure.
 */
/**
 * @brief Manages both the main read-write database and the read-only seed database.
 *
 * DatabaseManager is the **sole class** aware of the dual-table architecture:
 * - @c meagremud.db  -  read-write user database containing user tables, copies
 *   of the seeded world knowledge in @c Seeded_* tables, and the @c Meta table.
 * - @c seed.db  -  read-only, ships with MeagreMUD, produced by @c meagre-scrape.
 *
 * On startup, DatabaseManager compares @c SeedMeta.seed_version from
 * @c seed.db against @c Meta.applied_seed_version in @c meagremud.db. If they
 * differ, all @c Seeded_* tables are silently dropped and repopulated.
 *
 * ## Access pattern
 * All world-knowledge reads go through the merged views @c v_Room, @c v_Monster,
 * @c v_Item, @c v_RoomExit, and @c v_RoomMonster. User records win over seeded
 * records when @c source_id matches. Writes always go to user tables only.
 *
 * ## Thread safety
 * DatabaseManager is **not thread-safe**. Use it from a single thread only
 * (the daemon's main thread). CharacterSession worker threads receive
 * configuration via queued signals rather than querying the database directly.
 *
 * @see Schema, CharacterRegistry
 */
class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    /// Named connection for the main read-write database (meagremud.db).
    static constexpr const char *MAIN_CONNECTION = "meagre_main";

    /// Named connection for the read-only seed database (seed.db).
    static constexpr const char *SEED_CONNECTION = "meagre_seed";

    /**
     * @brief Constructs the DatabaseManager.
     * @param parent Optional Qt parent object.
     */
    explicit DatabaseManager(QObject *parent = nullptr);

    /**
     * @brief Destructor. Closes both database connections if open.
     */
    ~DatabaseManager() override;

    /**
     * @brief Opens both databases and prepares the schema.
     *
     * Opens @c mainDbPath as the read-write user database, creates the full
     * schema on first run, runs pending migrations, and optionally loads
     * @c seedDbPath. If the seed version has changed the Seeded_* tables are
     * reloaded silently.
     *
     * @param mainDbPath Filesystem path to @c meagremud.db.
     * @param seedDbPath Filesystem path to @c seed.db. Pass an empty string
     *                   to skip seed loading.
     * @return @c true if @c meagremud.db was opened and the schema is current.
     */
    bool open(const QString &mainDbPath, const QString &seedDbPath = QString());

    /**
     * @brief Closes both database connections and releases all resources.
     */
    void close();

    /**
     * @brief Returns @c true if the main database is open.
     */
    bool isOpen() const;

    /**
     * @brief Returns the main read-write database connection.
     *
     * Do not cache the returned object across calls  -  connections can be
     * invalidated by close().
     *
     * @return A QSqlDatabase handle to @c meagremud.db.
     */
    QSqlDatabase mainDb() const;

    /**
     * @brief Executes a plain SQL statement on the main database.
     *
     * Logs a warning on failure.
     *
     * @param sql SQL statement to execute.
     * @return @c true on success, @c false on error.
     */
    bool exec(const QString &sql);

    /**
     * @brief Executes a parameterised SQL statement on the main database.
     *
     * Parameters are bound positionally (index 0, 1, 2, ...) from @p bindings.
     * Logs a warning on failure.
     *
     * @param sql      Prepared SQL statement (use @c ? placeholders).
     * @param bindings Values to bind, in positional order.
     * @return @c true on success, @c false on error.
     */
    bool exec(const QString &sql, const QVariantList &bindings);

    /**
     * @brief Reads a value from the @c Meta table.
     *
     * @param key          The meta key to look up.
     * @param defaultValue Value to return if the key is not found.
     * @return The stored value, or @p defaultValue if not present.
     */
    QString metaValue(const QString &key,
                      const QString &defaultValue = QString()) const;

    /**
     * @brief Writes or updates a value in the @c Meta table.
     *
     * Uses @c INSERT OR REPLACE semantics.
     *
     * @param key   The meta key.
     * @param value The value to store.
     * @return @c true on success.
     */
    bool setMetaValue(const QString &key, const QString &value);

    /**
     * @brief Returns the current applied schema version from the Meta table.
     * @return Integer schema version, or 0 if the Meta table is empty.
     */
    int schemaVersion() const;

    /**
     * @brief Returns @c true if @c seed.db was successfully opened and applied.
     */
    bool seedLoaded() const { return m_seedLoaded; }

    /**
     * @brief Returns the seed version string that was last applied.
     *
     * Returns an empty string if no seed has ever been applied.
     */
    QString appliedSeedVersion() const;

signals:
    /**
     * @brief Emitted after the Seeded_* tables have been reloaded from seed.db.
     * @param newVersion The seed_version string from the new seed.
     */
    void seedReloaded(const QString &newVersion);

private:
    bool openMainDb(const QString &path);
    bool openSeedDb(const QString &path);
    bool createSchema();
    bool runMigrations();
    bool checkAndReloadSeed();
    bool reloadSeedTables();

    static bool execOnDb(QSqlDatabase &db, const QString &sql);

    bool    m_open       = false; ///< True if the main database is open.
    bool    m_seedLoaded = false; ///< True if seed.db was loaded successfully.
};
