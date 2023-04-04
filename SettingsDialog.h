#ifndef __SETTINGS
#define __SETTINGS

#include <QDialog>
#include <QtNetwork/QNetworkAddressEntry>

QT_USE_NAMESPACE

QT_BEGIN_NAMESPACE

class QLineEdit;
class QSpinBox;

QT_END_NAMESPACE

namespace MeagreMUD
{
class Settings;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(Settings *currentSettings, QWidget *parent = nullptr);
    virtual ~SettingsDialog();

signals:
    void settingsUpdated(MeagreMUD::Settings *newSettings);

public Q_SLOTS:
    void acceptSettings();

private slots:

private:
    QLineEdit *hostLineEdit;
    QSpinBox *portSpinBox;
    Settings *settings;
};
}

#endif
