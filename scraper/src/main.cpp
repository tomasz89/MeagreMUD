#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFileInfo>

#include "db/DatabaseManager.h"
#include "NativeDbImporter.h"
#include "WorldKnowledgeWriter.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("meagre-scrape"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("MeagreMUD"));

    // -----------------------------------------------------------------------
    // Command line
    // -----------------------------------------------------------------------

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral(
            "meagre-scrape -- MajorMUD world knowledge importer\n\n"
            "Imports MajorMUD Btrieve .dat/.vir files into seed.db.\n"
            "File paths are supplied explicitly to avoid filename guessing.\n\n"
            "Example (NMR _dats directory):\n"
            "  meagre-scrape import \\\n"
            "    --db seed.db \\\n"
            "    --rooms  wccmp002.vir \\\n"
            "    --monsters wccknms2.vir \\\n"
            "    --items  wccitem2.vir"));
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument(
        QStringLiteral("command"),
        QStringLiteral("Command to run. Currently: import"));

    QCommandLineOption dbOption(
        QStringList() << QStringLiteral("d") << QStringLiteral("db"),
        QStringLiteral("Path to seed.db output file (created if absent)."),
        QStringLiteral("path"),
        QStringLiteral("seed.db"));
    parser.addOption(dbOption);

    QCommandLineOption serverIdOption(
        QStringList() << QStringLiteral("s") << QStringLiteral("server-id"),
        QStringLiteral("MudServer.id to associate imported records with."),
        QStringLiteral("id"),
        QStringLiteral("1"));
    parser.addOption(serverIdOption);

    QCommandLineOption roomsOption(
        QStringList() << QStringLiteral("r") << QStringLiteral("rooms"),
        QStringLiteral("Path to the room data file (e.g. wccmp002.vir)."),
        QStringLiteral("file"));
    parser.addOption(roomsOption);

    QCommandLineOption monstersOption(
        QStringList() << QStringLiteral("m") << QStringLiteral("monsters"),
        QStringLiteral("Path to the monster data file (e.g. wccknms2.vir)."),
        QStringLiteral("file"));
    parser.addOption(monstersOption);

    QCommandLineOption itemsOption(
        QStringList() << QStringLiteral("i") << QStringLiteral("items"),
        QStringLiteral("Path to the item data file (e.g. wccitem2.vir)."),
        QStringLiteral("file"));
    parser.addOption(itemsOption);

    QCommandLineOption seedVersionOption(
        QStringList() << QStringLiteral("seed-version"),
        QStringLiteral("Seed version string written to SeedMeta (default: 1.0.0)."),
        QStringLiteral("version"),
        QStringLiteral("1.0.0"));
    parser.addOption(seedVersionOption);

    QCommandLineOption mudVersionOption(
        QStringList() << QStringLiteral("mud-version"),
        QStringLiteral("MajorMUD version string for SeedMeta (default: 1.11p)."),
        QStringLiteral("version"),
        QStringLiteral("1.11p"));
    parser.addOption(mudVersionOption);

    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty())
    {
        parser.showHelp(1);
        return 1;
    }

    const QString command = positional.at(0);
    if (command != QStringLiteral("import"))
    {
        qCritical() << "Unknown command:" << command
                    << "(currently only 'import' is supported)";
        return 1;
    }

    const QString dbPath      = parser.value(dbOption);
    const int     serverId    = parser.value(serverIdOption).toInt();
    const QString seedVersion = parser.value(seedVersionOption);
    const QString mudVersion  = parser.value(mudVersionOption);

    NativeDbImporter::FilePaths paths;
    paths.roomFile     = parser.value(roomsOption);
    paths.monsterFile  = parser.value(monstersOption);
    paths.itemFile     = parser.value(itemsOption);

    if (paths.roomFile.isEmpty()
        && paths.monsterFile.isEmpty()
        && paths.itemFile.isEmpty())
    {
        qCritical() << "No input files specified."
                    << "Use --rooms, --monsters, and/or --items.";
        parser.showHelp(1);
        return 1;
    }

    if (serverId <= 0)
    {
        qCritical() << "Invalid --server-id:" << serverId;
        return 1;
    }

    // Warn about any specified files that don't exist
    auto checkFile = [](const QString &path, const QString &name) -> bool
    {
        if (path.isEmpty())
        {
            return true; // not specified -- skipped, not an error
        }
        if (!QFileInfo::exists(path))
        {
            qCritical() << name << "file not found:" << path;
            return false;
        }
        return true;
    };

    if (!checkFile(paths.roomFile,    QStringLiteral("Room"))
     || !checkFile(paths.monsterFile, QStringLiteral("Monster"))
     || !checkFile(paths.itemFile,    QStringLiteral("Item")))
    {
        return 1;
    }

    // -----------------------------------------------------------------------
    // Open seed.db
    // -----------------------------------------------------------------------

    qInfo() << "meagre-scrape: opening" << dbPath;

    DatabaseManager db;
    if (!db.open(dbPath))
    {
        qCritical() << "Failed to open seed.db:" << dbPath;
        return 1;
    }

    // -----------------------------------------------------------------------
    // Import
    // -----------------------------------------------------------------------

    qInfo() << "meagre-scrape: starting import";
    if (!paths.roomFile.isEmpty())
    {
        qInfo() << "  rooms:    " << paths.roomFile;
    }
    if (!paths.monsterFile.isEmpty())
    {
        qInfo() << "  monsters: " << paths.monsterFile;
    }
    if (!paths.itemFile.isEmpty())
    {
        qInfo() << "  items:    " << paths.itemFile;
    }

    NativeDbImporter importer(db, serverId);

    QObject::connect(&importer, &NativeDbImporter::progress,
                     [](const QString &stage, int current, int total)
                     {
                         if (current % 500 == 0 || current == total)
                         {
                             qInfo().noquote()
                                 << QString(QStringLiteral("  %1: %2 / %3"))
                                        .arg(stage, -10)
                                        .arg(current)
                                        .arg(total);
                         }
                     });

    const NativeDbImporter::ImportResult result = importer.import(paths);

    qInfo() << "\nImport summary:";
    qInfo() << "  Rooms:    " << result.roomsImported;
    qInfo() << "  Monsters: " << result.monstersImported;
    qInfo() << "  Items:    " << result.itemsImported;
    qInfo() << "  Exits:    " << result.exitsImported;
    qInfo() << "  Spawns:   " << result.spawnRecordsImported;

    if (!result.warnings.isEmpty())
    {
        qInfo() << result.warnings.size() << "warnings:";
        for (const QString &w : result.warnings)
        {
            qWarning() << " " << w;
        }
    }

    // -----------------------------------------------------------------------
    // Finalise
    // -----------------------------------------------------------------------

    WorldKnowledgeWriter writer(db);
    if (!writer.finalise(seedVersion, mudVersion))
    {
        qCritical() << "Failed to finalise seed.db";
        return 1;
    }

    qInfo() << "\nmeagre-scrape: done. seed.db written to" << dbPath;
    return 0;
}
