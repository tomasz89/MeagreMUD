#include <QCoreApplication>
#include <QCommandLineParser>
#include <QHostAddress>
#include <QDebug>

#include "DaemonServer.h"
#include "CharacterRegistry.h"
#include "db/DatabaseManager.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("meagre-daemon"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("MeagreMUD"));

    // -----------------------------------------------------------------------
    // Command line parsing
    // -----------------------------------------------------------------------

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("MeagreMUD daemon — manages character sessions"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(
        QStringList() << QStringLiteral("p") << QStringLiteral("port"),
        QStringLiteral("Port to listen on for GUI connections (default: 7777)."),
        QStringLiteral("port"),
        QStringLiteral("7777"));
    parser.addOption(portOption);

    QCommandLineOption bindOption(
        QStringList() << QStringLiteral("b") << QStringLiteral("bind"),
        QStringLiteral("Address to bind to (default: 127.0.0.1)."),
        QStringLiteral("address"),
        QStringLiteral("127.0.0.1"));
    parser.addOption(bindOption);

    parser.process(app);

    const quint16 port = static_cast<quint16>(
        parser.value(portOption).toUShort());
    const QHostAddress bindAddress(parser.value(bindOption));

    if (port == 0)
    {
        qCritical() << "Invalid port number.";
        return 1;
    }

    // -----------------------------------------------------------------------
    // Startup
    // -----------------------------------------------------------------------

    qInfo() << "MeagreMUD daemon starting up…";

    // -----------------------------------------------------------------------
    // Database
    // -----------------------------------------------------------------------

    const QString mainDbPath = QStringLiteral("meagremud.db");
    const QString seedDbPath = QStringLiteral("seed.db");

    DatabaseManager db;
    if (!db.open(mainDbPath, seedDbPath))
    {
        qCritical() << "Failed to open database — aborting.";
        return 1;
    }

    // -----------------------------------------------------------------------
    // Server
    // -----------------------------------------------------------------------

    CharacterRegistry registry;
    DaemonServer server(&registry);

    if (!server.listen(bindAddress, port))
    {
        return 1;
    }

    // TODO: load character sessions from database and start them
    // For now the daemon starts with no sessions; GUIs can connect and
    // receive an empty resync dump.

    qInfo() << "MeagreMUD daemon ready.";

    return app.exec();
}
