#include <QCoreApplication>
#include <QCommandLineParser>
#include <QHostAddress>
#include <QSqlError>
#include <QSqlQuery>
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

    QSqlQuery query(db.mainDb());
    const QString loadCharactersSql = QStringLiteral(
        "SELECT \"Character\".id AS char_db_id, \"Character\".name AS char_name, "
        "MudServer.host AS mud_host, MudServer.port AS mud_port, "
        "MudServer.id AS server_id, MudInstance.id AS instance_id, "
        "MudServer.mud_type AS mud_type, MudInstance.instance_selector AS instance_selector, "
        "UserLogin.username AS login_username, UserLogin.password AS login_password, "
        "\"Character\".auto_reconnect AS auto_reconnect "
        "FROM \"Character\" "
        "JOIN MudInstance ON \"Character\".instance_id = MudInstance.id "
        "JOIN UserLogin ON MudInstance.login_id = UserLogin.id "
        "JOIN MudServer ON UserLogin.server_id = MudServer.id "
        "ORDER BY \"Character\".id ASC "
        "LIMIT 16"
    );

    if (!query.exec(loadCharactersSql))
    {
        qCritical() << "Failed to query characters:" << query.lastError().text();
        return 1;
    }

    CharacterRegistry registry;
    DaemonServer server(&registry);

    int protocolId = 1;
    while (query.next())
    {
        const int charDbId = query.value("char_db_id").toInt();
        const QString name = query.value("char_name").toString();
        const QString host = query.value("mud_host").toString();
        const quint16 portValue = static_cast<quint16>(query.value("mud_port").toUInt());
        const QString loginUsername = query.value("login_username").toString();
        const QString loginPassword = query.value("login_password").toString();
        const bool autoReconnect = query.value("auto_reconnect").toBool();
        const quint8 serverProfileId = static_cast<quint8>(query.value("server_id").toUInt());
        const quint8 instanceId = static_cast<quint8>(query.value("instance_id").toUInt());

        if (protocolId > 16)
        {
            qWarning() << "Main: ignoring character" << name
                       << "because only 16 characters are supported.";
            break;
        }

        registry.addSession(
            static_cast<quint8>(protocolId), name,
            charDbId,
            host, portValue,
            loginUsername, loginPassword,
            autoReconnect,
            serverProfileId, instanceId);
        protocolId++;
    }

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
