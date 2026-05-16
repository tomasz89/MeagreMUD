#pragma once

/**
 * @file WorldKnowledgeWriter.h
 * @brief Finalises seed.db after import: writes SeedMeta and vacuum.
 */

#include <QObject>
#include <QString>
#include <QDateTime>

class DatabaseManager;

/**
 * @brief Writes the SeedMeta record and performs final seed.db housekeeping.
 *
 * Called after NativeDbImporter completes. Sets the seed_version string
 * that DatabaseManager uses at runtime to decide whether to reload the
 * Seeded_* tables into meagremud.db.
 */
class WorldKnowledgeWriter : public QObject
{
    Q_OBJECT

public:
    explicit WorldKnowledgeWriter(DatabaseManager &db,
                                  QObject *parent = nullptr);

    /**
     * @brief Write SeedMeta and run VACUUM on seed.db.
     *
     * @param seedVersion    Version string e.g. "1.0.0".
     * @param sourceVersion  MajorMUD version string e.g. "1.11p".
     * @return true on success.
     */
    bool finalise(const QString &seedVersion,
                  const QString &sourceVersion);

private:
    DatabaseManager &m_db;
};
