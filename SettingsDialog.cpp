#include "SettingsDialog.h"

#include <QIntValidator>
#include <QGroupBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDialogButtonBox>

QT_USE_NAMESPACE

static const char blankString[] = QT_TRANSLATE_NOOP("SettingsDialog", "N/A");

MeagreMUD::SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    formGroupBox(new QGroupBox(tr("Connection Settings")))
{
    createTCPSettingsForm();

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *mainLayout = new QVBoxLayout;

    mainLayout->addWidget(formGroupBox);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    setWindowTitle(tr("Settings"));

}

void MeagreMUD::SettingsDialog::createTCPSettingsForm()
{
    QFormLayout *layout = new QFormLayout;
    layout->addRow(new QLabel(tr("Host:")), new QLineEdit);
    QSpinBox *portSpinBox = new QSpinBox();
    portSpinBox->setMinimum(0);
    portSpinBox->setMinimum(65535);
    portSpinBox->setValue(23);
    layout->addRow(new QLabel(tr("Port:")), portSpinBox);
    formGroupBox->setLayout(layout);
}

MeagreMUD::SettingsDialog::~SettingsDialog()
{
    delete formGroupBox;
    delete buttonBox;
}

MeagreMUD::SettingsDialog::Settings MeagreMUD::SettingsDialog::settings() const
{
    return currentSettings;
}

#if 0
void SettingsDialog::apply()
{
    updateSettings();
    hide();
}
#endif
