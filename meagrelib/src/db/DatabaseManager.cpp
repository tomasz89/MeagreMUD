#include "db/DatabaseManager.h"
#include "db/Schema.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include <QFile>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

// ---------------------------------------------------------------------------
// Open / close
// ---------------------------------------------------------------------------

bool DatabaseManager::open(const QString &mainDbPath, const QString &seedDbPath)
{
    if (m_open)
    {
        qWarning() << "DatabaseManager::open() called when already open";
        return true;
    }

    if (!openMainDb(mainDbPath))
    {
        return false;
    }

    if (!createSchema())
    {
        return false;
    }

    if (!runMigrations())
    {
        return false;
    }

    if (!seedDbPath.isEmpty() && QFile::exists(seedDbPath))
    {
        if (openSeedDb(seedDbPath))
        {
            checkAndReloadSeed();
        }
    }
    else if (!seedDbPath.isEmpty())
    {
        qWarning() << "DatabaseManager: seed.db not found at" << seedDbPath;
    }

    m_open = true;
    qInfo() << "DatabaseManager: opened" << mainDbPath
            << "(schema v" << schemaVersion() << ")";
    return true;
}

void DatabaseManager::close()
{
    if (!m_open)
    {
        return;
    }

    {
        QSqlDatabase main = QSqlDatabase::database(MAIN_CONNECTION, false);
        if (main.isOpen())
        {
            main.close();
        }
    }
    QSqlDatabase::removeDatabase(QString::fromLatin1(MAIN_CONNECTION));

    {
        QSqlDatabase seed = QSqlDatabase::database(SEED_CONNECTION, false);
        if (seed.isOpen())
        {
            seed.close();
        }
    }
    QSqlDatabase::removeDatabase(QString::fromLatin1(SEED_CONNECTION));

    m_open       = false;
    m_seedLoaded = false;
}

bool DatabaseManager::isOpen() const
{
    return m_open;
}

QSqlDatabase DatabaseManager::mainDb() const
{
    return QSqlDatabase::database(QString::fromLatin1(MAIN_CONNECTION));
}

// ---------------------------------------------------------------------------
// Query helpers
// ---------------------------------------------------------------------------

bool DatabaseManager::exec(const QString &sql)
{
    QSqlDatabase db = mainDb();
    QSqlQuery query(db);
    if (!query.exec(sql))
    {
        qWarning() << "DatabaseManager::exec() failed:"
                   << query.lastError().text()
                   << "\nSQL:" << sql;
        return false;
    }
    return true;
}

bool DatabaseManager::exec(const QString &sql, const QVariantList &bindings)
{
    QSqlDatabase db = mainDb();
    QSqlQuery query(db);
    query.prepare(sql);
    for (int i = 0; i < bindings.size(); ++i)
    {
        query.bindValue(i, bindings.at(i));
    }
    if (!query.exec())
    {
        qWarning() << "DatabaseManager::exec(bindings) failed:"
                   << query.lastError().text()
                   << "\nSQL:" << sql;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Meta table
// ---------------------------------------------------------------------------

QString DatabaseManager::metaValue(const QString &key,
                                   const QString &defaultValue) const
{
    QSqlDatabase db = QSqlDatabase::database(
        QString::fromLatin1(MAIN_CONNECTION));
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT value FROM Meta WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec() || !query.next())
    {
        return defaultValue;
    }
    return query.value(0).toString();
}

bool DatabaseManager::setMetaValue(const QString &key, const QString &value)
{
    return exec(
        QStringLiteral(
            "INSERT INTO Meta (key, value) VALUES (?, ?)"
            " ON CONFLICT(key) DO UPDATE SET value = excluded.value"),
        {key, value});
}

int DatabaseManager::schemaVersion() const
{
    return metaValue(QStringLiteral("db_schema_version"),
                     QStringLiteral("0")).toInt();
}

QString DatabaseManager::appliedSeedVersion() const
{
    return metaValue(QStringLiteral("applied_seed_version"));
}

// ---------------------------------------------------------------------------
// Private — open databases
// ---------------------------------------------------------------------------

bool DatabaseManager::openMainDb(const QString &path)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"),
        QString::fromLatin1(MAIN_CONNECTION));
    db.setDatabaseName(path);

    if (!db.open())
    {
        qCritical() << "DatabaseManager: failed to open main database:"
                    << db.lastError().text();
        QSqlDatabase::removeDatabase(QString::fromLatin1(MAIN_CONNECTION));
        return false;
    }

    // Enable WAL mode and foreign keys
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    q.exec(QStringLiteral("PRAGMA foreign_keys=ON"));

    return true;
}

bool DatabaseManager::openSeedDb(const QString &path)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"),
        QString::fromLatin1(SEED_CONNECTION));
    db.setDatabaseName(path);
    // Open read-only via URI
    db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_URI;"
                                        "QSQLITE_OPEN_READONLY"));

    if (!db.open())
    {
        qWarning() << "DatabaseManager: failed to open seed database:"
                   << db.lastError().text();
        QSqlDatabase::removeDatabase(QString::fromLatin1(SEED_CONNECTION));
        return false;
    }

    qDebug() << "DatabaseManager: seed.db opened from" << path;
    return true;
}

// ---------------------------------------------------------------------------
// Private — schema creation
// ---------------------------------------------------------------------------

bool DatabaseManager::createSchema()
{
    QSqlDatabase db = mainDb();

    db.transaction();

    int i = 0;
    while (Schema::ALL_CREATE_STATEMENTS[i] != nullptr)
    {
        const QString sql = QString::fromLatin1(Schema::ALL_CREATE_STATEMENTS[i]);
        if (!execOnDb(db, sql))
        {
            qCritical() << "DatabaseManager: schema creation failed at statement"
                        << i;
            db.rollback();
            return false;
        }
        ++i;
    }

    db.commit();

    // Set schema version if not already set
    if (schemaVersion() == 0)
    {
        setMetaValue(QStringLiteral("db_schema_version"),
                     QString::number(Schema::CURRENT_SCHEMA_VERSION));
    }

    return true;
}

// ---------------------------------------------------------------------------
// Private — migrations
// ---------------------------------------------------------------------------

bool DatabaseManager::runMigrations()
{
    const int current = schemaVersion();

    if (current == Schema::CURRENT_SCHEMA_VERSION)
    {
        return true;
    }

    if (current > Schema::CURRENT_SCHEMA_VERSION)
    {
        qWarning() << "DatabaseManager: database schema version" << current
                   << "is newer than application version"
                   << Schema::CURRENT_SCHEMA_VERSION
                   << "— proceeding with caution";
        return true;
    }

    qInfo() << "DatabaseManager: migrating schema from version"
            << current << "to" << Schema::CURRENT_SCHEMA_VERSION;

    // Migration scripts are applied in order from (current+1) to CURRENT.
    // Add cases here as the schema evolves.
    for (int v = current + 1; v <= Schema::CURRENT_SCHEMA_VERSION; ++v)
    {
        switch (v)
        {
            // Version 1 is the initial schema — no migration needed,
            // createSchema() handles it.
            case 1:
                break;

            default:
                qCritical() << "DatabaseManager: no migration script for"
                               " schema version" << v;
                return false;
        }

        setMetaValue(QStringLiteral("db_schema_version"), QString::number(v));
        qInfo() << "DatabaseManager: migrated to schema version" << v;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Private — seed reload
// ---------------------------------------------------------------------------

bool DatabaseManager::checkAndReloadSeed()
{
    QSqlDatabase seedDb = QSqlDatabase::database(
        QString::fromLatin1(SEED_CONNECTION));
    if (!seedDb.isOpen())
    {
        return false;
    }

    // Read seed version from seed.db
    QSqlQuery q(seedDb);
    if (!q.exec(QStringLiteral("SELECT seed_version FROM SeedMeta LIMIT 1"))
        || !q.next())
    {
        qWarning() << "DatabaseManager: could not read SeedMeta from seed.db";
        return false;
    }

    const QString seedVersion = q.value(0).toString();
    const QString appliedVersion = appliedSeedVersion();

    if (seedVersion == appliedVersion)
    {
        qDebug() << "DatabaseManager: seed version" << seedVersion
                 << "already applied — no reload needed";
        m_seedLoaded = true;
        return true;
    }

    QString fromVersion = appliedVersion;
    if (fromVersion.isEmpty())
    {
        fromVersion = QStringLiteral("(none)");
    }
    qInfo() << "DatabaseManager: seed version changed from"
            << fromVersion << "to" << seedVersion << "— reloading";

    return reloadSeedTables();
}

bool DatabaseManager::reloadSeedTables()
{
    QSqlDatabase seedDb = QSqlDatabase::database(
        QString::fromLatin1(SEED_CONNECTION));
    if (!seedDb.isOpen())
    {
        return false;
    }

    QSqlDatabase mainDb = this->mainDb();
    mainDb.transaction();

    // Drop existing seeded tables
    const QStringList seededTables = {
        QStringLiteral("Seeded_RoomMonster"),
        QStringLiteral("Seeded_RoomExit"),
        QStringLiteral("Seeded_Room"),
        QStringLiteral("Seeded_Monster"),
        QStringLiteral("Seeded_Item"),
        QStringLiteral("SeedMeta"),
    };

    for (const QString &table : seededTables)
    {
        if (!execOnDb(mainDb,
                      QStringLiteral("DELETE FROM %1").arg(table)))
        {
            qCritical() << "DatabaseManager: failed to clear" << table;
            mainDb.rollback();
            return false;
        }
    }

    // Attach seed.db and copy tables across
    // SQLite ATTACH allows cross-database INSERT SELECT
    const QString seedPath = seedDb.databaseName();
    if (!execOnDb(mainDb,
                  QStringLiteral("ATTACH DATABASE '%1' AS seed")
                      .arg(seedPath)))
    {
        qCritical() << "DatabaseManager: ATTACH seed.db failed";
        mainDb.rollback();
        return false;
    }

    const QStringList copyStatements = {
        QStringLiteral("INSERT INTO Seeded_Room SELECT * FROM seed.Seeded_Room"),
        QStringLiteral("INSERT INTO Seeded_RoomExit SELECT * FROM seed.Seeded_RoomExit"),
        QStringLiteral("INSERT INTO Seeded_Monster SELECT * FROM seed.Seeded_Monster"),
        QStringLiteral("INSERT INTO Seeded_Item SELECT * FROM seed.Seeded_Item"),
        QStringLiteral("INSERT INTO Seeded_RoomMonster SELECT * FROM seed.Seeded_RoomMonster"),
        QStringLiteral("INSERT INTO SeedMeta SELECT * FROM seed.SeedMeta"),
    };

    bool ok = true;
    for (const QString &stmt : copyStatements)
    {
        if (!execOnDb(mainDb, stmt))
        {
            qCritical() << "DatabaseManager: seed copy failed:" << stmt;
            ok = false;
            break;
        }
    }

    execOnDb(mainDb, QStringLiteral("DETACH DATABASE seed"));

    if (!ok)
    {
        mainDb.rollback();
        return false;
    }

    // Read the new seed version from the just-copied SeedMeta
    QSqlQuery versionQuery(mainDb);
    versionQuery.exec(
        QStringLiteral("SELECT seed_version FROM SeedMeta LIMIT 1"));
    QString newVersion;
    if (versionQuery.next())
    {
        newVersion = versionQuery.value(0).toString();
    }
    else
    {
        newVersion = QStringLiteral("unknown");
    }

    setMetaValue(QStringLiteral("applied_seed_version"), newVersion);

    mainDb.commit();

    m_seedLoaded = true;
    qInfo() << "DatabaseManager: seed reload complete — version" << newVersion;
    emit seedReloaded(newVersion);
    return true;
}

// ---------------------------------------------------------------------------
// Static helper
// ---------------------------------------------------------------------------

bool DatabaseManager::execOnDb(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery query(db);
    if (!query.exec(sql))
    {
        qWarning() << "DatabaseManager::execOnDb failed:"
                   << query.lastError().text()
                   << "\nSQL:" << sql;
        return false;
    }
    return true;
}
