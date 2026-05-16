#include "WorldKnowledgeWriter.h"
#include "db/DatabaseManager.h"

#include <QDebug>

WorldKnowledgeWriter::WorldKnowledgeWriter(DatabaseManager &db,
                                           QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

bool WorldKnowledgeWriter::finalise(const QString &seedVersion,
                                    const QString &sourceVersion)
{
    // Write SeedMeta
    const bool ok = m_db.exec(
        QStringLiteral(
            "INSERT INTO SeedMeta (seed_version, generated_at, source_version) "
            "VALUES (?, datetime('now'), ?)"),
        {seedVersion, sourceVersion});

    if (!ok)
    {
        qCritical() << "WorldKnowledgeWriter: failed to write SeedMeta";
        return false;
    }

    // Compact the database
    m_db.exec(QStringLiteral("VACUUM"));

    qInfo() << "WorldKnowledgeWriter: seed.db finalised"
            << "seed_version=" << seedVersion
            << "source_version=" << sourceVersion;

    return true;
}
