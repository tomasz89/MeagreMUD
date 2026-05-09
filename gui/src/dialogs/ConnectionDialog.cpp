#include "dialogs/ConnectionDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ConnectionDialog::ConnectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Connect to Daemon"));
    setMinimumSize(400, 300);
    resize(450, 350);
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    // Profile selection
    auto *profileGroup = new QGroupBox(QStringLiteral("Profile"), this);
    auto *profileLayout = new QHBoxLayout(profileGroup);

    m_profileCombo = new QComboBox(profileGroup);
    profileLayout->addWidget(new QLabel(QStringLiteral("Profile:"), profileGroup));
    profileLayout->addWidget(m_profileCombo, 1);

    auto *addButton = new QPushButton(QStringLiteral("Add New..."), profileGroup);
    profileLayout->addWidget(addButton);

    layout->addWidget(profileGroup);

    // Connection settings
    auto *connGroup = new QGroupBox(QStringLiteral("Connection"), this);
    auto *connForm = new QFormLayout(connGroup);

    m_hostEdit = new QLineEdit(connGroup);
    m_hostEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));
    connForm->addRow(QStringLiteral("Host:"), m_hostEdit);

    m_portSpin = new QSpinBox(connGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(DEFAULT_PORT);
    connForm->addRow(QStringLiteral("Port:"), m_portSpin);

    layout->addWidget(connGroup);

    // Authentication
    auto *authGroup = new QGroupBox(QStringLiteral("Authentication"), this);
    auto *authLayout = new QVBoxLayout(authGroup);

    m_authCheck = new QCheckBox(QStringLiteral("Requires authentication"), authGroup);
    authLayout->addWidget(m_authCheck);

    auto *tokenRow = new QWidget(authGroup);
    auto *tokenLayout = new QHBoxLayout(tokenRow);
    tokenLayout->setContentsMargins(0, 0, 0, 0);

    m_tokenLabel = new QLabel(QStringLiteral("Token:"), tokenRow);
    tokenLayout->addWidget(m_tokenLabel);

    m_tokenEdit = new QLineEdit(tokenRow);
    m_tokenEdit->setEchoMode(QLineEdit::Password);
    tokenLayout->addWidget(m_tokenEdit, 1);

    authLayout->addWidget(tokenRow);
    layout->addWidget(authGroup);

    // Buttons
    m_buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);
    auto *connectButton = m_buttons->addButton(QStringLiteral("Connect"),
                                               QDialogButtonBox::ActionRole);
    auto *disconnectButton = m_buttons->addButton(QStringLiteral("Disconnect"),
                                                  QDialogButtonBox::ActionRole);
    layout->addWidget(m_buttons);

    // Connections
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionDialog::onProfileChanged);
    connect(addButton, &QPushButton::clicked, this, &ConnectionDialog::onAddProfile);
    connect(connectButton, &QPushButton::clicked, this, &ConnectionDialog::onConnect);
    connect(disconnectButton, &QPushButton::clicked, this, &ConnectionDialog::onDisconnect);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Update token visibility
    auto updateTokenVisibility = [this]() {
        bool auth = m_authCheck->isChecked();
        m_tokenLabel->setEnabled(auth);
        m_tokenEdit->setEnabled(auth);
    };
    connect(m_authCheck, &QCheckBox::toggled, this, updateTokenVisibility);
    updateTokenVisibility();

    // Load profiles
    loadProfiles();
    if (!m_profiles.isEmpty()) {
        m_profileCombo->setCurrentText(m_profiles.first());
        onProfileChanged();
    }
}

ConnectionDialog::~ConnectionDialog() = default;

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void ConnectionDialog::onProfileChanged()
{
    QString profile = m_profileCombo->currentText();
    if (!profile.isEmpty()) {
        loadProfileSettings(profile);
    }
}

void ConnectionDialog::onAddProfile()
{
    bool ok;
    QString newProfile = QInputDialog::getText(this, QStringLiteral("New Profile"),
                                               QStringLiteral("Profile name:"),
                                               QLineEdit::Normal, QString(), &ok);
    if (ok && !newProfile.isEmpty()) {
        if (m_profiles.contains(newProfile)) {
            QMessageBox::warning(this, QStringLiteral("Duplicate Profile"),
                                 QStringLiteral("Profile '%1' already exists.").arg(newProfile));
            return;
        }
        m_profiles.append(newProfile);
        saveProfiles();
        m_profileCombo->addItem(newProfile);
        m_profileCombo->setCurrentText(newProfile);
        // Clear fields for new profile
        m_hostEdit->setText(QStringLiteral("127.0.0.1"));
        m_portSpin->setValue(DEFAULT_PORT);
        m_authCheck->setChecked(false);
        m_tokenEdit->clear();
    }
}

void ConnectionDialog::onConnect()
{
    QString profile = m_profileCombo->currentText();
    if (profile.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("No Profile"),
                             QStringLiteral("Please select or add a profile."));
        return;
    }
    saveProfileSettings(profile);
    emit connectRequested(profile);
    accept();
}

void ConnectionDialog::onDisconnect()
{
    emit disconnectRequested();
    accept();
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void ConnectionDialog::loadProfiles()
{
    QSettings s = makeSettings(QStringLiteral("main"));
    m_profiles = s.value(KEY_PROFILES, QStringList()).toStringList();
    m_profileCombo->clear();
    m_profileCombo->addItems(m_profiles);
}

void ConnectionDialog::saveProfiles()
{
    QSettings s = makeSettings(QStringLiteral("main"));
    s.setValue(KEY_PROFILES, m_profiles);
    s.sync();
}

void ConnectionDialog::loadProfileSettings(const QString &profile)
{
    QSettings s = makeSettings(profile);
    m_hostEdit->setText(s.value(KEY_HOST, QStringLiteral("127.0.0.1")).toString());
    m_portSpin->setValue(s.value(KEY_PORT, DEFAULT_PORT).toInt());
    m_authCheck->setChecked(s.value(KEY_AUTH_REQUIRED, false).toBool());
    m_tokenEdit->setText(s.value(KEY_AUTH_TOKEN, QString()).toString());
}

void ConnectionDialog::saveProfileSettings(const QString &profile)
{
    QSettings s = makeSettings(profile);
    s.setValue(KEY_HOST, m_hostEdit->text().trimmed());
    s.setValue(KEY_PORT, m_portSpin->value());
    s.setValue(KEY_AUTH_REQUIRED, m_authCheck->isChecked());
    s.setValue(KEY_AUTH_TOKEN, m_tokenEdit->text());
}

QSettings ConnectionDialog::makeSettings(const QString &profile)
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("MeagreMUD"),
                     QStringLiteral("MeagreMUD-%1").arg(profile));
}