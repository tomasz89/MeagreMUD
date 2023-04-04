#include "Settings.h"

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <QSettings>

QT_USE_NAMESPACE

MeagreMUD::Settings::Settings(QObject *parent) :
    QObject(parent),
    qSettings(new QSettings(this)),
    settingsFileName(),
    hostName("greatermud.com"),
    hostPort(23)
{
}

MeagreMUD::Settings::~Settings()
{
}

void MeagreMUD::Settings::setHost(const QString &host)
{
    hostName = host;
    qSettings->setValue("connection/host", host);
}

void MeagreMUD::Settings::setPort(const quint16 port)
{
    hostPort = port;
    qSettings->setValue("connection/port", port);
}

const QString &MeagreMUD::Settings::host() const
{
    return hostName;
}

quint16 MeagreMUD::Settings::port() const
{
    return hostPort;
}

void MeagreMUD::Settings::setSettingsFileName(const QString &fileName)
{
    settingsFileName = fileName;

    //TODO does nothing
}
