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
    setWindowTitle(QStringLiteral("Daemon Connection"));
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

    auto *removeButton = new QPushButton(QStringLiteral("Remove"), profileGroup);
    profileLayout->addWidget(removeButton);
    m_removeButton = removeButton;

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
    auto *saveButton = new QPushButton(QStringLiteral("Save"), this);
    m_saveButton = saveButton;
    m_buttons->addButton(saveButton, QDialogButtonBox::ActionRole);
    auto *connectButton = m_buttons->addButton(QStringLiteral("Connect"),
                                               QDialogButtonBox::ActionRole);
    auto *disconnectButton = m_buttons->addButton(QStringLiteral("Disconnect"),
                                                  QDialogButtonBox::ActionRole);
    layout->addWidget(m_buttons);

    // Connections
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionDialog::onProfileChanged);
    connect(addButton, &QPushButton::clicked, this, &ConnectionDialog::onAddProfile);
    connect(removeButton, &QPushButton::clicked, this, &ConnectionDialog::onRemoveProfile);
    connect(saveButton, &QPushButton::clicked, this, &ConnectionDialog::onSaveProfile);
    connect(connectButton, &QPushButton::clicked, this, &ConnectionDialog::onConnect);
    connect(disconnectButton, &QPushButton::clicked, this, &ConnectionDialog::onDisconnect);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Update token visibility and save button state
    auto updateTokenVisibility = [this]() {
        bool auth = m_authCheck->isChecked();
        m_tokenLabel->setEnabled(auth);
        m_tokenEdit->setEnabled(auth);
        updateButtonStates();
    };
    connect(m_authCheck, &QCheckBox::toggled, this, updateTokenVisibility);
    connect(m_hostEdit, &QLineEdit::textChanged, this, &ConnectionDialog::updateButtonStates);
    connect(m_portSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ConnectionDialog::updateButtonStates);
    connect(m_tokenEdit, &QLineEdit::textChanged, this, &ConnectionDialog::updateButtonStates);
    updateTokenVisibility();

    // Load profiles
    loadProfiles();
    if (!m_profiles.isEmpty()) {
        m_profileCombo->setCurrentText(m_profiles.first());
        onProfileChanged();
    }
    updateButtonStates();
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

        QString host = m_hostEdit->text();
        int port = m_portSpin->value();
        bool authRequired = m_authCheck->isChecked();
        QString token = m_tokenEdit->text();

        m_profiles.append(newProfile);
        saveProfiles();
        m_profileCombo->addItem(newProfile);
        m_profileCombo->setCurrentText(newProfile);

        m_hostEdit->setText(host);
        m_portSpin->setValue(port);
        m_authCheck->setChecked(authRequired);
        m_tokenEdit->setText(token);
        saveProfileSettings(newProfile);
        updateButtonStates();
    }
}

void ConnectionDialog::onSaveProfile()
{
    QString profile = m_profileCombo->currentText();
    if (profile.isEmpty()) {
        return;
    }

    saveProfileSettings(profile);
    updateButtonStates();
}

void ConnectionDialog::onRemoveProfile()
{
    QString profile = m_profileCombo->currentText();
    if (profile.isEmpty()) {
        return;
    }

    if (QMessageBox::question(this, QStringLiteral("Remove Profile"),
                              QStringLiteral("Remove profile '%1'? ").arg(profile)) != QMessageBox::Yes) {
        return;
    }

    int currentIndex = m_profileCombo->currentIndex();
    m_profiles.removeAll(profile);
    saveProfiles();
    removeProfileSettings(profile);
    m_profileCombo->removeItem(currentIndex);

    if (!m_profiles.isEmpty()) {
        m_profileCombo->setCurrentIndex(0);
        onProfileChanged();
    } else {
        m_hostEdit->setText(QStringLiteral("127.0.0.1"));
        m_portSpin->setValue(DEFAULT_PORT);
        m_authCheck->setChecked(false);
        m_tokenEdit->clear();
    }

    updateButtonStates();
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
    updateButtonStates();
}

void ConnectionDialog::saveProfileSettings(const QString &profile)
{
    QSettings s = makeSettings(profile);
    s.setValue(KEY_HOST, m_hostEdit->text().trimmed());
    s.setValue(KEY_PORT, m_portSpin->value());
    s.setValue(KEY_AUTH_REQUIRED, m_authCheck->isChecked());
    s.setValue(KEY_AUTH_TOKEN, m_tokenEdit->text());
    s.sync();
}

void ConnectionDialog::removeProfileSettings(const QString &profile)
{
    QSettings s = makeSettings(profile);
    s.clear();
    s.sync();
}

bool ConnectionDialog::currentSettingsMatchSaved() const
{
    QString profile = m_profileCombo->currentText();
    if (profile.isEmpty()) {
        return true;
    }

    QSettings s = makeSettings(profile);
    QString savedHost = s.value(KEY_HOST, QStringLiteral("127.0.0.1")).toString();
    int savedPort = s.value(KEY_PORT, DEFAULT_PORT).toInt();
    bool savedAuth = s.value(KEY_AUTH_REQUIRED, false).toBool();
    QString savedToken = s.value(KEY_AUTH_TOKEN, QString()).toString();

    return savedHost == m_hostEdit->text().trimmed()
        && savedPort == m_portSpin->value()
        && savedAuth == m_authCheck->isChecked()
        && savedToken == m_tokenEdit->text();
}

void ConnectionDialog::updateButtonStates()
{
    bool hasProfiles = !m_profiles.isEmpty();
    if (m_removeButton) {
        m_removeButton->setEnabled(hasProfiles);
    }
    if (m_saveButton) {
        m_saveButton->setEnabled(hasProfiles && !currentSettingsMatchSaved());
    }
}

QSettings ConnectionDialog::makeSettings(const QString &profile)
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("MeagreMUD"),
                     QStringLiteral("MeagreMUD-%1").arg(profile));
}