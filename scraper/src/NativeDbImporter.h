#pragma once

/**
 * @file NativeDbImporter.h
 * @brief Imports MajorMUD native Btrieve database files into seed.db.
 *
 * All field offset knowledge for MajorMUD v1.11p is private to
 * NativeDbImporter.cpp. Only this interface is public.
 */

#include <QObject>
#include <QString>
#include <QStringList>

class DatabaseManager;

/**
 * @brief Imports MajorMUD v1.11p Btrieve .dat files into the Seeded_*
 *        tables of seed.db via DatabaseManager.
 *
 * Field offsets are derived from Nightmare Redux modFieldmaps_vO.bas
 * (v1.11p / version O). All offset constants are in the Offsets namespace
 * inside NativeDbImporter.cpp -- only that file changes if the format
 * ever needs updating.
 *
 * Import is always a full replace: the Seeded_* tables are cleared and
 * repopulated. is_verified is set to false and fingerprint is set to NULL
 * for all imported rooms -- confirmed by live daemon play via relearn.
 *
 * Usage:
 * @code
 * NativeDbImporter importer(db, serverId);
 * auto result = importer.import("/path/to/mmud/data");
 * qInfo() << "Imported" << result.roomsImported << "rooms";
 * @endcode
 */
class NativeDbImporter : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Summary of a completed import operation.
     */
    struct ImportResult {
        int          roomsImported   = 0;
        int          monstersImported = 0;
        int          itemsImported   = 0;
        int          exitsImported   = 0;
        int          spawnRecordsImported = 0;
        QStringList  warnings;
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
     * @brief Run the full import from a MajorMUD data directory.
     *
     * Expects the following files to exist in @p sourceDir:
     *   - w<CC>mp002.dat  (rooms)
     *   - w<CC>knms2.dat  (monsters)
     *   - w<CC>knit2.dat  (items)
     *
     * where @p callLetters is the BBS call-letter prefix (e.g. "00", "WCC").
     *
     * Clears all Seeded_* tables before importing. Runs inside a single
     * database transaction -- rolls back entirely on any error.
     *
     * @param sourceDir    Directory containing the .dat files.
     * @param callLetters  BBS call-letter prefix for file names (default "00").
     * @return             ImportResult summary including any warnings.
     */
    ImportResult import(const QString &sourceDir,
                        const QString &callLetters = QStringLiteral("00"));

signals:
    /**
     * @brief Emitted periodically during import with progress information.
     * @param stage    Current stage name (e.g. "Rooms", "Monsters").
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
