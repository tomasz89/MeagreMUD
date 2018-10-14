#ifndef __SETTINGS
#define __SETTINGS

#include <QDialog>
#include <QtNetwork/QNetworkAddressEntry>

QT_USE_NAMESPACE

QT_BEGIN_NAMESPACE

class QIntValidator;
class QGroupBox;
class QDialogButtonBox;

QT_END_NAMESPACE

namespace MeagreMUD
{
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    struct Settings {
        QString host;
        quint16 port;
    };

    explicit SettingsDialog(QWidget *parent = nullptr);
    virtual ~SettingsDialog();

    Settings settings() const;

private slots:

private:
    void createTCPSettingsForm();
private:
    QGroupBox *formGroupBox;
    QDialogButtonBox *buttonBox;
    Settings currentSettings;
    QIntValidator *intValidator;
};
}

#endif
