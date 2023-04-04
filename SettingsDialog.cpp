#include "SettingsDialog.h"
#include "Settings.h"

#include <QGroupBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDialogButtonBox>

QT_USE_NAMESPACE

MeagreMUD::SettingsDialog::SettingsDialog(MeagreMUD::Settings *currentSettings, QWidget *parent) :
    QDialog(parent),
    hostLineEdit(0),
    portSpinBox(0),
    settings(currentSettings)
{
    QFormLayout *layout = new QFormLayout;
    hostLineEdit = new QLineEdit(settings->host());
    layout->addRow(new QLabel(tr("Host:")), hostLineEdit);
    portSpinBox = new QSpinBox();
    portSpinBox->setRange(1, 65535);
    portSpinBox->setValue(settings->port());
    layout->addRow(new QLabel(tr("Port:")), portSpinBox);

    QGroupBox *formGroupBox = new QGroupBox(tr("Connection Settings"));
    formGroupBox->setLayout(layout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *mainLayout = new QVBoxLayout;

    mainLayout->addWidget(formGroupBox);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    setWindowTitle(tr("Settings"));

    connect(this, SIGNAL(accepted()), this, SLOT(acceptSettings()));

}

MeagreMUD::SettingsDialog::~SettingsDialog()
{
}

void MeagreMUD::SettingsDialog::acceptSettings()
{
    settings->setHost(hostLineEdit->text());
    settings->setPort(portSpinBox->value());

    emit settingsUpdated(settings);
}
