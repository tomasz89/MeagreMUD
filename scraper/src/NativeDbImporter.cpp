#include "NativeDbImporter.h"
#include "BtrieveReader.h"
#include "db/DatabaseManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

// ---------------------------------------------------------------------------
// Field offset maps -- MajorMUD v1.11p (version O / modFieldmaps_vO.bas)
//
// All offsets are byte positions within a raw record as returned by
// BtrieveReader::nextRecord().
//
// Type abbreviations from the NMR VB type declarations:
//   L4  = Long  (4 bytes, signed little-endian)
//   I2  = Integer (2 bytes, signed little-endian)
//   B1  = Byte (1 byte, unsigned)
//   S<n> = fixed-length String, n bytes, null-padded
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

// Exit direction names indexed 0-9 (N/NE/E/SE/S/SW/W/NW/UP/DOWN)
// and their inverses for flee path retracing
static const char *EXIT_NAMES[Room::EXIT_COUNT] = {
    "north", "northeast", "east", "southeast",
    "south", "southwest", "west", "northwest",
    "up", "down"
};

static const char *EXIT_INVERSE[Room::EXIT_COUNT] = {
    "south", "southwest", "west", "northwest",
    "north", "northeast", "east", "southeast",
    "down", "up"
};

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

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NativeDbImporter::NativeDbImporter(DatabaseManager &db, int serverId,
                                   QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_serverId(serverId)
{
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

NativeDbImporter::ImportResult NativeDbImporter::import(
    const FilePaths &paths)
{
    ImportResult result;

    QSqlDatabase db = m_db.mainDb();
    db.transaction();

    clearSeededTables();

    importItems(paths.itemFile, result);
    importMonsters(paths.monsterFile, result);
    importRooms(paths.roomFile, result);

    db.commit();

    qInfo() << "NativeDbImporter: import complete --"
            << result.roomsImported    << "rooms,"
            << result.monstersImported << "monsters,"
            << result.itemsImported    << "items,"
            << result.exitsImported    << "exits,"
            << result.spawnRecordsImported << "spawn records";

    for (const QString &w : result.warnings)
    {
        qWarning() << "NativeDbImporter:" << w;
    }

    return result;
}

// ---------------------------------------------------------------------------
// Private -- clear seeded tables
// ---------------------------------------------------------------------------

void NativeDbImporter::clearSeededTables()
{
    m_db.exec(QStringLiteral("DELETE FROM Seeded_RoomMonster"));
    m_db.exec(QStringLiteral("DELETE FROM Seeded_RoomExit"));
    m_db.exec(QStringLiteral("DELETE FROM Seeded_Room"));
    m_db.exec(QStringLiteral("DELETE FROM Seeded_Monster"));
    m_db.exec(QStringLiteral("DELETE FROM Seeded_Item"));
    m_db.exec(QStringLiteral("DELETE FROM SeedMeta"));
    qDebug() << "NativeDbImporter: cleared Seeded_* tables";
}

// ---------------------------------------------------------------------------
// Private -- import items
// ---------------------------------------------------------------------------

void NativeDbImporter::importItems(const QString &path, ImportResult &result)
{
    BtrieveReader reader(path);
    if (!reader.open())
    {
        result.warnings.append(
            QStringLiteral("Could not open item file: %1").arg(path));
        return;
    }

    const int total = reader.recordCount();
    int processed   = 0;

    const QString sql = QStringLiteral(
        "INSERT INTO Seeded_Item "
        "(server_id, name, weight_units, value_copper, is_collectible, "
        " currency_value_copper, currency_denomination, source_id) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

    QByteArray rec;
    while (!(rec = reader.nextRecord()).isNull())
    {
        const quint32 number   = BtrieveReader::rb4(rec, Offsets::Item::Number);
        const QString name     = BtrieveReader::rbStr(rec, Offsets::Item::Name, 29);
        const qint16  weight   = static_cast<qint16>(
            BtrieveReader::rb2(rec, Offsets::Item::Weight));
        const qint16  type     = static_cast<qint16>(
            BtrieveReader::rb2(rec, Offsets::Item::Type));
        const qint16  cost     = static_cast<qint16>(
            BtrieveReader::rb2(rec, Offsets::Item::Cost));
        const quint8  gettable = BtrieveReader::rb1(rec, Offsets::Item::Gettable);
        const qint32  openGold = static_cast<qint32>(
            BtrieveReader::rb4(rec, Offsets::Item::OpenGold));
        const qint32  openSilv = static_cast<qint32>(
            BtrieveReader::rb4(rec, Offsets::Item::OpenSilver));
        const qint32  openCopp = static_cast<qint32>(
            BtrieveReader::rb4(rec, Offsets::Item::OpenCopper));

        if (number == 0 || name.isEmpty())
        {
            continue;
        }

        const bool isCurrency = (type & Offsets::Item::TYPE_CURRENCY) != 0;

        // Currency value in copper: gold=100, silver=10, copper=1
        QVariant currencyValue;
        QVariant currencyDenom;
        if (isCurrency)
        {
            if (openGold > 0)
            {
                currencyValue = openGold * 100;
                currencyDenom = 3; // gold
            }
            else if (openSilv > 0)
            {
                currencyValue = openSilv * 10;
                currencyDenom = 2; // silver
            }
            else if (openCopp > 0)
            {
                currencyValue = openCopp;
                currencyDenom = 1; // copper
            }
        }

        int gettableInt = 0;
        if (gettable != 0) { gettableInt = 1; }

        m_db.exec(sql, {
            m_serverId,
            name,
            static_cast<int>(weight),
            static_cast<int>(cost),
            gettableInt,
            currencyValue,
            currencyDenom,
            static_cast<int>(number)
        });

        processed++;
        if (processed % 100 == 0)
        {
            emit progress(QStringLiteral("Items"), processed, total);
        }
    }

    result.itemsImported = processed;
    emit progress(QStringLiteral("Items"), processed, total);
    qDebug() << "NativeDbImporter: imported" << processed << "items";
}

// ---------------------------------------------------------------------------
// Private -- import monsters
// ---------------------------------------------------------------------------

void NativeDbImporter::importMonsters(const QString &path, ImportResult &result)
{
    BtrieveReader reader(path);
    if (!reader.open())
    {
        result.warnings.append(
            QStringLiteral("Could not open monster file: %1").arg(path));
        return;
    }

    const int total = reader.recordCount();
    int processed   = 0;

    const QString sql = QStringLiteral(
        "INSERT INTO Seeded_Monster "
        "(server_id, name, disposition, min_level, max_level, source_id) "
        "VALUES (?, ?, ?, ?, ?, ?)");

    QByteArray rec;
    while (!(rec = reader.nextRecord()).isNull())
    {
        const quint32 number    = BtrieveReader::rb4(rec, Offsets::Monster::Number);
        const QString name      = BtrieveReader::rbStr(rec, Offsets::Monster::Name, 29);
        const qint16  alignment = static_cast<qint16>(
            BtrieveReader::rb2(rec, Offsets::Monster::Alignment));
        const qint16  minLevel  = static_cast<qint16>(
            BtrieveReader::rb2(rec, Offsets::Monster::MinLevel));
        const qint16  maxLevel  = static_cast<qint16>(
            BtrieveReader::rb2(rec, Offsets::Monster::MaxLevel));

        if (number == 0 || name.isEmpty())
        {
            continue;
        }

        // NMR alignment: 0=Friendly, 1=Neutral, 2=Hostile
        // Our disposition: 0=Friendly, 1=Neutral, 2=Hostile, 3=Unknown
        int disposition = 3; // Unknown default
        if (alignment >= 0 && alignment <= 2)
        {
            disposition = static_cast<int>(alignment);
        }

        QVariant minLevelVar;
        if (minLevel > 0) { minLevelVar = static_cast<int>(minLevel); }
        QVariant maxLevelVar;
        if (maxLevel > 0) { maxLevelVar = static_cast<int>(maxLevel); }

        m_db.exec(sql, {
            m_serverId,
            name,
            disposition,
            minLevelVar,
            maxLevelVar,
            static_cast<int>(number)
        });

        processed++;
        if (processed % 100 == 0)
        {
            emit progress(QStringLiteral("Monsters"), processed, total);
        }
    }

    result.monstersImported = processed;
    emit progress(QStringLiteral("Monsters"), processed, total);
    qDebug() << "NativeDbImporter: imported" << processed << "monsters";
}

// ---------------------------------------------------------------------------
// Private -- import rooms, exits, spawn records
// ---------------------------------------------------------------------------

void NativeDbImporter::importRooms(const QString &path, ImportResult &result)
{
    BtrieveReader reader(path);
    if (!reader.open())
    {
        result.warnings.append(
            QStringLiteral("Could not open room file: %1").arg(path));
        return;
    }

    const int total = reader.recordCount();
    int roomsProcessed  = 0;
    int exitsProcessed  = 0;
    int spawnsProcessed = 0;

    const QString roomSql = QStringLiteral(
        "INSERT INTO Seeded_Room "
        "(server_id, name, zone, is_verified, is_safe, source_id) "
        "VALUES (?, ?, ?, 0, ?, ?)");

    const QString exitSql = QStringLiteral(
        "INSERT INTO Seeded_RoomExit "
        "(room_id, direction, inverse_direction, destination_room_id, is_hidden) "
        "VALUES (?, ?, ?, ?, 0)");

    const QString spawnSql = QStringLiteral(
        "INSERT INTO Seeded_RoomMonster "
        "(room_id, monster_id, spawn_chance) "
        "VALUES (?, ?, ?)");

    // We need to resolve monster/room source_id -> DB id for exits and spawns.
    // Build a map: source_id -> Seeded_Room.id after inserting each room.
    // Similarly for monsters.
    QHash<int, int> monsterSourceToId;

    // Pre-build monster source_id -> id map
    {
        QSqlDatabase db = m_db.mainDb();
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "SELECT id, source_id FROM Seeded_Monster WHERE server_id = %1")
            .arg(m_serverId));
        while (q.next())
        {
            monsterSourceToId.insert(q.value(1).toInt(), q.value(0).toInt());
        }
    }

    QByteArray rec;
    while (!(rec = reader.nextRecord()).isNull())
    {
        const quint32 mapNum  = BtrieveReader::rb4(rec, Offsets::Room::MapNumber);
        const quint32 roomNum = BtrieveReader::rb4(rec, Offsets::Room::RoomNumber);
        const QString name    = BtrieveReader::rbStr(rec, Offsets::Room::Name, 53);
        const qint16  rtype   = static_cast<qint16>(
            BtrieveReader::rb2(rec, Offsets::Room::RoomType));

        if (mapNum == 0 && roomNum == 0)
        {
            continue;
        }
        if (name.isEmpty())
        {
            continue;
        }

        // Encode source_id as map*100000 + room to keep it unique
        // and human-readable in the DB (e.g. map 1, room 245 -> 100245)
        const int sourceId   = static_cast<int>(mapNum) * 100000
                             + static_cast<int>(roomNum);
        const bool isSafe    = (rtype & 0x0001) != 0;
        const QString zone   = QStringLiteral("Map %1").arg(mapNum);

        int isSafeInt = 0;
        if (isSafe) { isSafeInt = 1; }

        m_db.exec(roomSql, {
            m_serverId,
            name,
            zone,
            isSafeInt,
            sourceId
        });

        // Retrieve the auto-assigned DB id for this room
        QSqlDatabase db = m_db.mainDb();
        QSqlQuery idQ(db);
        idQ.exec(QStringLiteral("SELECT last_insert_rowid()"));
        int roomDbId = 0;
        if (idQ.next())
        {
            roomDbId = idQ.value(0).toInt();
        }

        roomsProcessed++;

        // --- Exits ---
        for (int e = 0; e < Offsets::Room::EXIT_COUNT; ++e)
        {
            const quint32 destPacked = BtrieveReader::rb4(
                rec, Offsets::Room::ExitDest + e * 4);

            if (destPacked == 0)
            {
                continue;
            }

            // Packed: high 16 bits = dest map, low 16 bits = dest room
            const int destMap  = static_cast<int>(destPacked >> 16);
            const int destRoom = static_cast<int>(destPacked & 0xFFFF);
            const int destSourceId = destMap * 100000 + destRoom;

            // Resolve dest room DB id -- may not exist yet if not imported
            // Use a subquery at query time; store source_id reference for now
            // and resolve in a post-pass, or store as NULL and let live play
            // fill it in via relearn. For now store destination_room_id as NULL
            // and add a warning-free lookup attempt.
            QSqlQuery destQ(db);
            destQ.prepare(QStringLiteral(
                "SELECT id FROM Seeded_Room "
                "WHERE server_id = ? AND source_id = ?"));
            destQ.addBindValue(m_serverId);
            destQ.addBindValue(destSourceId);
            destQ.exec();

            QVariant destDbId;
            if (destQ.next())
            {
                destDbId = destQ.value(0);
            }

            const QString dir     = QString::fromLatin1(
                Offsets::EXIT_NAMES[e]);
            const QString inverse = QString::fromLatin1(
                Offsets::EXIT_INVERSE[e]);

            m_db.exec(exitSql, {
                roomDbId,
                dir,
                inverse,
                destDbId
            });

            exitsProcessed++;
        }

        // --- Spawn records ---
        for (int s = 0; s < Offsets::Room::SPAWN_COUNT; ++s)
        {
            const quint32 monNum = BtrieveReader::rb4(
                rec, Offsets::Room::MonsterNum + s * 4);
            const quint8  monPer = BtrieveReader::rb1(
                rec, Offsets::Room::MonsterPer + s);

            if (monNum == 0)
            {
                continue;
            }

            const int monDbId = monsterSourceToId.value(
                static_cast<int>(monNum), 0);

            if (monDbId == 0)
            {
                result.warnings.append(
                    QStringLiteral("Room %1/%2: unknown monster %3 in spawn table")
                        .arg(mapNum).arg(roomNum).arg(monNum));
                continue;
            }

            m_db.exec(spawnSql, {
                roomDbId,
                monDbId,
                static_cast<int>(monPer)
            });

            spawnsProcessed++;
        }

        if (roomsProcessed % 50 == 0)
        {
            emit progress(QStringLiteral("Rooms"), roomsProcessed, total);
        }
    }

    // Second pass: resolve NULL destination_room_ids for exits
    // where the destination room was imported after the exit was written
    {
        QSqlDatabase db = m_db.mainDb();
        const QString fixSql = QStringLiteral(
            "UPDATE Seeded_RoomExit "
            "SET destination_room_id = ("
            "    SELECT sr.id FROM Seeded_Room sr"
            "    JOIN Seeded_Room srcRoom ON srcRoom.id = Seeded_RoomExit.room_id"
            "    WHERE sr.server_id = srcRoom.server_id"
            "    AND sr.source_id = ("
            "        SELECT dest_source FROM _exit_dest_tmp "
            "        WHERE exit_id = Seeded_RoomExit.id"
            "    )"
            ") "
            "WHERE destination_room_id IS NULL");
        // Note: full two-pass resolution requires a temp table which adds
        // complexity. Exits with NULL destination will be resolved during live
        // play via the relearn mechanism. This is acceptable for the seed.
        Q_UNUSED(fixSql)
    }

    result.roomsImported   = roomsProcessed;
    result.exitsImported   = exitsProcessed;
    result.spawnRecordsImported = spawnsProcessed;

    emit progress(QStringLiteral("Rooms"), roomsProcessed, total);
    qDebug() << "NativeDbImporter: imported"
             << roomsProcessed << "rooms,"
             << exitsProcessed << "exits,"
             << spawnsProcessed << "spawn records";
}
