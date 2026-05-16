#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
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
            "Usage: meagre-scrape import --db <seed.db> --server-id <id>"
            " [--call-letters <cc>] [--seed-version <ver>] <source_dir>"));
    parser.addHelpOption();
    parser.addVersionOption();

    // Subcommand positional argument
    parser.addPositionalArgument(
        QStringLiteral("command"),
        QStringLiteral("Command to run. Currently: import"));

    parser.addPositionalArgument(
        QStringLiteral("source_dir"),
        QStringLiteral("Directory containing MajorMUD .dat files"));

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

    QCommandLineOption callLettersOption(
        QStringList() << QStringLiteral("c") << QStringLiteral("call-letters"),
        QStringLiteral("BBS call-letter prefix for .dat file names (default: 00)."),
        QStringLiteral("cc"),
        QStringLiteral("00"));
    parser.addOption(callLettersOption);

    QCommandLineOption seedVersionOption(
        QStringList() << QStringLiteral("v") << QStringLiteral("seed-version"),
        QStringLiteral("Seed version string written to SeedMeta (default: 1.0.0)."),
        QStringLiteral("version"),
        QStringLiteral("1.0.0"));
    parser.addOption(seedVersionOption);

    QCommandLineOption sourceVersionOption(
        QStringList() << QStringLiteral("m") << QStringLiteral("mud-version"),
        QStringLiteral("MajorMUD version string for SeedMeta (default: 1.11p)."),
        QStringLiteral("version"),
        QStringLiteral("1.11p"));
    parser.addOption(sourceVersionOption);

    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    if (positional.size() < 2)
    {
        parser.showHelp(1);
        return 1;
    }

    const QString command    = positional.at(0);
    const QString sourceDir  = positional.at(1);
    const QString dbPath     = parser.value(dbOption);
    const int     serverId   = parser.value(serverIdOption).toInt();
    const QString callLetters = parser.value(callLettersOption);
    const QString seedVersion = parser.value(seedVersionOption);
    const QString mudVersion  = parser.value(sourceVersionOption);

    if (command != QStringLiteral("import"))
    {
        qCritical() << "Unknown command:" << command;
        parser.showHelp(1);
        return 1;
    }

    if (!QDir(sourceDir).exists())
    {
        qCritical() << "Source directory does not exist:" << sourceDir;
        return 1;
    }

    if (serverId <= 0)
    {
        qCritical() << "Invalid server-id:" << serverId;
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

    qInfo() << "meagre-scrape: importing from" << sourceDir
            << "call-letters=" << callLetters
            << "server-id=" << serverId;

    NativeDbImporter importer(db, serverId);

    QObject::connect(&importer, &NativeDbImporter::progress,
                     [](const QString &stage, int current, int total)
                     {
                         qInfo() << stage << current << "/" << total;
                     });

    const NativeDbImporter::ImportResult result =
        importer.import(sourceDir, callLetters);

    if (!result.warnings.isEmpty())
    {
        qInfo() << result.warnings.size() << "warnings:";
        for (const QString &w : result.warnings)
        {
            qWarning() << " " << w;
        }
    }

    qInfo() << "Import summary:";
    qInfo() << "  Rooms:    " << result.roomsImported;
    qInfo() << "  Monsters: " << result.monstersImported;
    qInfo() << "  Items:    " << result.itemsImported;
    qInfo() << "  Exits:    " << result.exitsImported;
    qInfo() << "  Spawns:   " << result.spawnRecordsImported;

    // -----------------------------------------------------------------------
    // Finalise
    // -----------------------------------------------------------------------

    WorldKnowledgeWriter writer(db);
    if (!writer.finalise(seedVersion, mudVersion))
    {
        qCritical() << "Failed to finalise seed.db";
        return 1;
    }

    qInfo() << "meagre-scrape: done. seed.db written to" << dbPath;
    return 0;
}
