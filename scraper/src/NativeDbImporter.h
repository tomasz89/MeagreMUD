#pragma once  
/**
 * @file NativeDbImporter.h
 * @brief Imports MajorMUD v1.11p Btrieve .dat/.vir files into the Seeded_*
 *        tables of seed.db via DatabaseManager.
 */  
#include <QObject>
#include <QString>
#include <QStringList>  
#include "BtrieveReader.h"

class DatabaseManager;  

// ---------------------------------------------------------------------------
// Field offset maps -- MajorMUD v1.11p (version O / modFieldmaps_vO.bas)
// ---------------------------------------------------------------------------  
namespace Offsets {  
// ---------------------------------------------------------------------------
// Room record (mp002.dat) -- record length 1544
// ---------------------------------------------------------------------------
namespace Room {
constexpr int MapNumber     =    0;  // L4  -- map/zone number
constexpr int RoomNumber    =    4;  // L4  -- room number within map
constexpr int Name          =  261;  // S53 -- room name
constexpr int Description   =  314;  // S512 -- room description (not imported)  
// Exit destination room numbers: 10 slots (N/NE/E/SE/S/SW/W/NW/UP/DOWN)
// Each is a packed value: high word = map number, low word = room number
// A value of 0 means no exit in that direction.
constexpr int ExitDest      =  824;  // L4 x 10  (40 bytes)  
// Exit type flags for each of the 10 directions
constexpr int ExitType      =  864;  // I2 x 10  (20 bytes)  
// Monster spawn slots: up to 15 monsters per room
constexpr int MonsterNum    = 1024;  // L4 x 15  (60 bytes) -- monster number
constexpr int MonsterPer    = 1144;  // B1 x 15  (15 bytes) -- spawn percentage  
// Room flags/type
constexpr int RoomType      = 1086;  // I2 -- bitmask; bit 0 = safe room  
constexpr int RECORD_SIZE   = 1544;
constexpr int EXIT_COUNT    = 10;
constexpr int SPAWN_COUNT   = 15;
}  
// ---------------------------------------------------------------------------
// Monster record (knms2.dat) -- record length 756
// ---------------------------------------------------------------------------
namespace Monster {
constexpr int Number        =   0;  // L4
constexpr int Name          =  54;  // S29
constexpr int Experience    = 116;  // L4
constexpr int Hitpoints     = 120;  // I2
constexpr int MinLevel      = 124;  // I2
constexpr int MaxLevel      = 126;  // I2
constexpr int Alignment     = 172;  // I2  0=Friendly 1=Neutral 2=Hostile
constexpr int ItemNumber    = 188;  // L4 x 10  -- loot item numbers
constexpr int ItemDropPer   = 252;  // B1 x 10  -- loot drop percentages
constexpr int Gold          = 272;  // L4  -- gold carried
constexpr int Silver        = 276;  // L4
constexpr int Copper        = 280;  // L4
constexpr int RECORD_SIZE   = 756;
constexpr int LOOT_COUNT    = 10;
}  
// ---------------------------------------------------------------------------
// Item record (knit2.dat) -- record length 1072
// ---------------------------------------------------------------------------
namespace Item {
constexpr int Number        =   0;  // L4
constexpr int Name          = 173;  // S29
constexpr int Weight        = 754;  // I2  -- weight in units
constexpr int Type          = 756;  // I2  -- item type bitmask
constexpr int Cost          = 802;  // I2  -- base cost in copper
constexpr int Gettable      = 924;  // B1  -- 1 = player can pick up
constexpr int OpenGold      = 952;  // L4  -- currency value (gold)
constexpr int OpenSilver    = 956;  // L4
constexpr int OpenCopper    = 960;  // L4
constexpr int RECORD_SIZE   = 1072;  
// Item type bitmask values (relevant subset)
constexpr int TYPE_CURRENCY = 0x0200;
}  
} // namespace Offsets

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
     * @param db        Open DatabaseManager pointing to seed.db.
     * @param serverId  MudServer.id to associate all imported records with.
     * @param order     The endianness of the source files (default BigEndian for MajorMUD).
     * @param parent    Qt parent object.
     */
    explicit NativeDbImporter(DatabaseManager &db, int serverId, 
                             BtrieveReader::ByteOrder order = BtrieveReader::ByteOrder::BigEndian,
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
    BtrieveReader::ByteOrder m_order; // Stores the endianness for the session
};
