#include "NativeDbImporter.h"
#include "BtrieveReader.h"
#include "db/DatabaseManager.h"  
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

// NOTE: Offsets namespace is provided by NativeDbImporter.h
// Do not redefine it here.

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------  

NativeDbImporter::NativeDbImporter(DatabaseManager &db, int serverId, BtrieveReader::ByteOrder order, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_serverId(serverId)
    , m_order(order)
{
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

NativeDbImporter::ImportResult NativeDbImporter::import(const FilePaths &paths)
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
            << result.itemsImported     << "items,"
            << result.exitsImported     << "exits,"
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
    BtrieveReader reader(path, m_order);
    if (!reader.open())
    {
        result.warnings.append(QStringLiteral("Could not open item file: %1").arg(path));
        return;
    }

    const int total = static_cast<int>(reader.recordCount());
    int processed   = 0;  
    const QString sql = QStringLiteral(
        "INSERT INTO Seeded_Item "
        "(server_id, name, weight_units, value_copper, is_collectible, "
        " currency_value_copper, currency_denomination, source_id) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");  

    QByteArray rec;
    while (!(rec = reader.nextRecord()).isEmpty())
    {
        const quint32 number   = BtrieveReader::assemble32(rec, Offsets::Item::Number, m_order);
        const QString name     = BtrieveReader::assembleStr(rec, Offsets::Item::Name, 29);
        const qint16  weight   = static_cast<qint16>(BtrieveReader::assemble16(rec, Offsets::Item::Weight, m_order));
        const qint16  type     = static_cast<qint16>(BtrieveReader::assemble16(rec, Offsets::Item::Type, m_order));
        const qint16  cost     = static_cast<qint16>(BtrieveReader::assemble16(rec, Offsets::Item::Cost, m_order));
        const quint8  gettable = BtrieveReader::assemble8(rec, Offsets::Item::Gettable, m_order);
        
        const quint32 gold   = BtrieveReader::assemble32(rec, Offsets::Item::OpenGold, m_order);
        const quint32 silver = BtrieveReader::assemble32(rec, Offsets::Item::OpenSilver, m_order);
        const quint32 copper = BtrieveReader::assemble32(rec, Offsets::Item::OpenCopper, m_order);
        const quint32 currencyValue = static_cast<quint32>(gold * 10000 + silver * 100 + copper);
        const quint32 currencyDenom = 1; 

        int gettableInt = (gettable != 0) ? 1 : 0;

        m_db.exec(sql, {
            m_serverId,
            name,
            static_cast<int>(weight),
            static_cast<int>(cost),
            gettableInt,
            static_cast<int>(currencyValue),
            static_cast<int>(currencyDenom),
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
    BtrieveReader reader(path, m_order);
    if (!reader.open())
    {
        result.warnings.append(QStringLiteral("Could not open monster file: %1").arg(path));
        return;
    }

    const int total = static_cast<int>(reader.recordCount());
    int processed   = 0;  
    const QString sql = QStringLiteral(
        "INSERT INTO Seeded_Monster "
        "(server_id, name, disposition, min_level, max_level, source_id) "
        "VALUES (?, ?, ?, ?, ?, ?)");  

    QByteArray rec;
    while (!(rec = reader.nextRecord()).isEmpty())
    {
        const quint32 number    = BtrieveReader::assemble32(rec, Offsets::Monster::Number, m_order);
        const QString name      = BtrieveReader::assembleStr(rec, Offsets::Monster::Name, 29);
        const qint16  alignment = static_cast<qint16>(BtrieveReader::assemble16(rec, Offsets::Monster::Alignment, m_order));
        const qint16  minLevel  = static_cast<qint16>(BtrieveReader::assemble16(rec, Offsets::Monster::MinLevel, m_order));
        const qint16  maxLevel  = static_cast<qint16>(BtrieveReader::assemble16(rec, Offsets::Monster::MaxLevel, m_order));  

        if (number == 0 || name.isEmpty())
        {
            continue;
        }

        int disposition = 3; 
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
    BtrieveReader reader(path, m_order);
    if (!reader.open())
    {
        result.warnings.append(QStringLiteral("Could not open room file: %1").arg(path));
        return;
    }

    const int total = static_cast<int>(reader.recordCount());
    int roomsProcessed  = 0;
    int exitsProcessed   = 0;
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

    QHash<int, int> monsterSourceToId;  
    QHash<int, int> roomSourceToId;

    // 1. Pre-build monster mapping
    {
        QSqlDatabase db = m_db.mainDb();
        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT id, source_id FROM Seeded_Monster WHERE server_id = %1").arg(m_serverId));
        while (q.next())
        {
            monsterSourceToId.insert(q.value(1).toInt(), q.value(0).toInt());
        }
    }

    // 2. Import Rooms
    QByteArray rec;
    while (!(rec = reader.nextRecord()).isEmpty())
    {
        const quint32 mapNum   = BtrieveReader::assemble32(rec, Offsets::Room::MapNumber, m_order);
        const quint32 roomNum  = BtrieveReader::assemble32(rec, Offsets::Room::RoomNumber, m_order);
        const QString name     = BtrieveReader::assembleStr(rec, Offsets::Room::Name, 53);
        const qint16  roomType = static_cast<qint16>(BtrieveReader::assemble16(rec, Offsets::Room::RoomType, m_order));
        
        if (roomNum == 0 || name.isEmpty()) continue;

        m_db.exec(roomSql, {
            m_serverId,
            name,
            static_cast<int>(mapNum),
            true, 
            static_cast<int>(roomNum)
        });

        roomSourceToId.insert(static_cast<int>(roomNum), 0); 
        roomsProcessed++;

        // 3. Import Exits (within room record)
        for (int e = 0; e < Offsets::Room::EXIT_COUNT; ++e)
        {
            const quint32 destPacked = BtrieveReader::assemble32(rec, Offsets::Room::ExitDest + (e * 4), m_order);
            if (destPacked == 0) continue;

            m_db.exec(exitSql, {
                0, 
                e, 
                0, 
                static_cast<int>(destPacked),
                0
            });
            exitsProcessed++;
        }

        // 4. Import Spawns (within room record)
        for (int s = 0; s < Offsets::Room::SPAWN_COUNT; ++s)
        {
            const quint32 monNum = BtrieveReader::assemble32(rec, Offsets::Room::MonsterNum + (s * 4), m_order);
            if (monNum == 0) continue;

            const quint8 monPer = BtrieveReader::assemble8(rec, Offsets::Room::MonsterPer + s, m_order);
            const int monDbId = monsterSourceToId.value(static_cast<int>(monNum), 0);

            if (monDbId != 0) 
            {
                m_db.exec(spawnSql, { 0, monDbId, static_cast<int>(monPer) });
                spawnsProcessed++;
            }
        }
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
