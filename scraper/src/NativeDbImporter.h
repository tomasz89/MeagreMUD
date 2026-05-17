#pragma once

/**
 * @file NativeDbImporter.h
 * @brief Imports MajorMUD native Btrieve database files into seed.db.
 */

#include <QObject>
#include <QString>
#include <QStringList>

class DatabaseManager;

/**
 * @brief Imports MajorMUD v1.11p Btrieve .dat/.vir files into the Seeded_*
 *        tables of seed.db via DatabaseManager.
 *
 * File paths are supplied explicitly -- no filename guessing is performed.
 * This makes the importer robust to different naming conventions across
 * MajorMUD distributions (e.g. .dat from NMR, .vir from raw installer).
 *
 * Field offsets are derived from Nightmare Redux modFieldmaps_vO.bas.
 * All offset constants are private to NativeDbImporter.cpp.
 */
class NativeDbImporter : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Explicit file paths for import.
     *
     * Any path left empty causes that entity type to be skipped with a warning.
     */
    struct FilePaths {
        QString roomFile;     ///< e.g. wccmp002.vir or wccmp002.dat
        QString monsterFile;  ///< e.g. wccknms2.vir or wccknms2.dat
        QString itemFile;     ///< e.g. wccitem2.vir or wccitem2.dat
    };

    /**
     * @brief Summary of a completed import operation.
     */
    struct ImportResult {
        int         roomsImported        = 0;
        int         monstersImported     = 0;
        int         itemsImported        = 0;
        int         exitsImported        = 0;
        int         spawnRecordsImported = 0;
        QStringList warnings;
    };

    /**
     * @brief Construct the importer.
     * @param db        Open DatabaseManager pointing at seed.db.
     * @param serverId  MudServer.id to associate all imported records with.
     * @param parent    Qt parent object.
     */
    explicit NativeDbImporter(DatabaseManager &db, int serverId,
                              QObject *parent = nullptr);

    /**
     * @brief Run the full import from explicitly specified file paths.
     *
     * Clears all Seeded_* tables before importing. Runs inside a single
     * database transaction -- rolls back entirely on any error.
     *
     * @param paths  Explicit paths to each required .dat/.vir file.
     * @return       ImportResult summary including any warnings.
     */
    ImportResult import(const FilePaths &paths);

signals:
    /**
     * @brief Emitted periodically during import with progress information.
     * @param stage    Current stage name (e.g. "Rooms", "Monsters", "Items").
     * @param current  Records processed so far in this stage.
     * @param total    Total records in this stage (from file header).
     */
    void progress(const QString &stage, int current, int total);

private:
    void clearSeededTables();
    void importRooms(const QString &path, ImportResult &result);
    void importMonsters(const QString &path, ImportResult &result);
    void importItems(const QString &path, ImportResult &result);

    DatabaseManager &m_db;
    int              m_serverId;
};
