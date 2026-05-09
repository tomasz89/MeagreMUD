#include "dialogs/SettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Settings"));
    setMinimumSize(640, 480);
    resize(720, 540);
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);
    m_tabs->setTabPosition(QTabWidget::West);
    m_tabs->addTab(buildServersAndCharactersTab(),
                   QStringLiteral("Servers && Characters"));
    m_tabs->addTab(buildDisplayTab(),
                   QStringLiteral("Display"));
    m_tabs->addTab(buildKeyMacrosTab(),
                   QStringLiteral("Key Macros"));
    m_tabs->addTab(buildTriggersTab(),
                   QStringLiteral("Triggers && Automation"));
    m_tabs->addTab(buildServerQuirksTab(),
                   QStringLiteral("Server Quirks"));
    m_tabs->addTab(buildDaemonConnectionTab(),
                   QStringLiteral("Daemon Connection"));
    layout->addWidget(m_tabs, 1);

    m_buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
        this);
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted,
            this, &SettingsDialog::onAccepted);
    connect(m_buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    connect(m_buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &SettingsDialog::onApply);

    loadProfiles();
    loadDaemonConnectionSettings();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SettingsDialog::setCurrentTab(Tab tab)
{
    m_tabs->setCurrentIndex(static_cast<int>(tab));
}

QString SettingsDialog::savedHost(const QString &profile)
{
    QSettings s = makeSettings(profile);
    return s.value(KEY_HOST, QStringLiteral("127.0.0.1")).toString();
}

quint16 SettingsDialog::savedPort(const QString &profile)
{
    QSettings s = makeSettings(profile);
    return static_cast<quint16>(
        s.value(KEY_PORT, DEFAULT_PORT).toUInt());
}

QString SettingsDialog::savedAuthToken(const QString &profile)
{
    QSettings s = makeSettings(profile);
    return s.value(KEY_AUTH_TOKEN, QString()).toString();
}

bool SettingsDialog::savedAuthRequired(const QString &profile)
{
    QSettings s = makeSettings(profile);
    return s.value(KEY_AUTH_REQUIRED, false).toBool();
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void SettingsDialog::onAccepted()
{
    onApply();
    accept();
}

void SettingsDialog::onApply()
{
    const bool changed = saveDaemonConnectionSettings();
    if (changed)
    {
        emit daemonConnectionChanged();
    }
}

// ---------------------------------------------------------------------------
// Tab builders — stubs
// ---------------------------------------------------------------------------

QWidget *SettingsDialog::buildServersAndCharactersTab()
{
    auto *w = new QWidget();
    auto *layout = new QVBoxLayout(w);
    layout->addWidget(new QLabel(
        QStringLiteral("Servers & Characters — not yet implemented."), w));
    layout->addStretch();
    return w;
}

QWidget *SettingsDialog::buildDisplayTab()
{
    auto *w = new QWidget();
    auto *layout = new QVBoxLayout(w);
    layout->addWidget(new QLabel(
        QStringLiteral("Display settings — not yet implemented."), w));
    layout->addStretch();
    return w;
}

QWidget *SettingsDialog::buildKeyMacrosTab()
{
    auto *w = new QWidget();
    auto *layout = new QVBoxLayout(w);
    layout->addWidget(new QLabel(
        QStringLiteral("Key Macros — not yet implemented."), w));
    layout->addStretch();
    return w;
}

QWidget *SettingsDialog::buildTriggersTab()
{
    auto *w = new QWidget();
    auto *layout = new QVBoxLayout(w);
    layout->addWidget(new QLabel(
        QStringLiteral("Triggers & Automation — not yet implemented."), w));
    layout->addStretch();
    return w;
}

QWidget *SettingsDialog::buildServerQuirksTab()
{
    auto *w = new QWidget();
    auto *layout = new QVBoxLayout(w);
    layout->addWidget(new QLabel(
        QStringLiteral("Server Quirks — not yet implemented."), w));
    layout->addStretch();
    return w;
}

// ---------------------------------------------------------------------------
// Tab 6 — Daemon Connection (fully implemented)
// ---------------------------------------------------------------------------

QWidget *SettingsDialog::buildDaemonConnectionTab()
{
    auto *w      = new QWidget();
    auto *layout = new QVBoxLayout(w);

    // Profile group
    auto *profileGroup = new QGroupBox(QStringLiteral("Profile"), w);
    auto *profileLayout = new QHBoxLayout(profileGroup);

    m_profileCombo = new QComboBox(profileGroup);
    m_profileCombo->addItems(m_profiles);
    profileLayout->addWidget(new QLabel(QStringLiteral("Profile:"), profileGroup));
    profileLayout->addWidget(m_profileCombo, 1);

    auto *addButton = new QPushButton(QStringLiteral("Add"), profileGroup);
    profileLayout->addWidget(addButton);

    auto *removeButton = new QPushButton(QStringLiteral("Remove"), profileGroup);
    profileLayout->addWidget(removeButton);

    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onProfileChanged);
    connect(addButton, &QPushButton::clicked, this, &SettingsDialog::onAddProfile);
    connect(removeButton, &QPushButton::clicked, this, &SettingsDialog::onRemoveProfile);

    layout->addWidget(profileGroup);

    // Connection group
    auto *connGroup  = new QGroupBox(QStringLiteral("Daemon Address"), w);
    auto *connForm   = new QFormLayout(connGroup);

    m_hostEdit = new QLineEdit(connGroup);
    m_hostEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));
    m_hostEdit->setMaxLength(253);  // max DNS hostname length
    connForm->addRow(QStringLiteral("Host:"), m_hostEdit);

    m_portSpin = new QSpinBox(connGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(DEFAULT_PORT);
    connForm->addRow(QStringLiteral("Port:"), m_portSpin);

    layout->addWidget(connGroup);

    // Authentication group
    auto *authGroup  = new QGroupBox(QStringLiteral("Authentication"), w);
    auto *authLayout = new QVBoxLayout(authGroup);

    m_authCheck = new QCheckBox(
        QStringLiteral("Daemon requires authentication"), authGroup);
    authLayout->addWidget(m_authCheck);

    auto *tokenRow    = new QWidget(authGroup);
    auto *tokenLayout = new QHBoxLayout(tokenRow);
    tokenLayout->setContentsMargins(0, 0, 0, 0);

    m_tokenLabel = new QLabel(QStringLiteral("Auth token:"), tokenRow);
    tokenLayout->addWidget(m_tokenLabel);

    m_tokenEdit = new QLineEdit(tokenRow);
    m_tokenEdit->setEchoMode(QLineEdit::Password);
    m_tokenEdit->setPlaceholderText(QStringLiteral("Token or password"));
    tokenLayout->addWidget(m_tokenEdit, 1);

    authLayout->addWidget(tokenRow);
    layout->addWidget(authGroup);

    // Show/hide token row based on auth checkbox
    auto updateTokenVisibility = [this]()
    {
        const bool authEnabled = m_authCheck->isChecked();
        m_tokenLabel->setEnabled(authEnabled);
        m_tokenEdit->setEnabled(authEnabled);
    };

    connect(m_authCheck, &QCheckBox::toggled, this,
            [updateTokenVisibility](bool)
            {
                updateTokenVisibility();
            });

    updateTokenVisibility();

    // Info note
    auto *noteLabel = new QLabel(
        QStringLiteral(
            "These settings are stored locally and are not part of the\n"
            "shared database. They are specific to this machine."),
        w);
    noteLabel->setWordWrap(true);
    QPalette notePalette = noteLabel->palette();
    notePalette.setColor(QPalette::WindowText,
                         noteLabel->palette().color(QPalette::Mid));
    noteLabel->setPalette(notePalette);
    layout->addWidget(noteLabel);

    layout->addStretch();
    return w;
}

void SettingsDialog::onProfileChanged()
{
    loadDaemonConnectionSettings();
}

void SettingsDialog::onAddProfile()
{
    // Similar to ConnectionDialog
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

void SettingsDialog::onRemoveProfile()
{
    QString profile = m_profileCombo->currentText();
    if (profile.isEmpty() || profile == QStringLiteral("Default")) {
        QMessageBox::warning(this, QStringLiteral("Cannot Remove"),
                             QStringLiteral("Cannot remove the Default profile."));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("Remove Profile"),
                              QStringLiteral("Remove profile '%1'?").arg(profile)) != QMessageBox::Yes) {
        return;
    }
    m_profiles.removeAll(profile);
    saveProfiles();
    m_profileCombo->removeItem(m_profileCombo->currentIndex());
    if (!m_profiles.isEmpty()) {
        m_profileCombo->setCurrentText(m_profiles.first());
        onProfileChanged();
    }
}

// ---------------------------------------------------------------------------
// Profile management
// ---------------------------------------------------------------------------

void SettingsDialog::loadProfiles()
{
    QSettings s = makeSettings(QStringLiteral("main"));
    m_profiles = s.value(QStringLiteral("profiles"), QStringList()).toStringList();
    if (m_profiles.isEmpty()) {
        m_profiles << QStringLiteral("Default");
        saveProfiles();
    }
}

void SettingsDialog::saveProfiles()
{
    QSettings s = makeSettings(QStringLiteral("main"));
    s.setValue(QStringLiteral("profiles"), m_profiles);
    s.sync();
}

QStringList SettingsDialog::profiles() const
{
    return m_profiles;
}

void SettingsDialog::setProfiles(const QStringList &profiles)
{
    m_profiles = profiles;
    saveProfiles();
}

void SettingsDialog::loadDaemonConnectionSettings()
{
    QString profile = m_profileCombo->currentText();
    if (profile.isEmpty() && !m_profiles.isEmpty()) {
        profile = m_profiles.first();
        m_profileCombo->setCurrentText(profile);
    }
    if (profile.isEmpty()) {
        profile = QStringLiteral("Default");
    }

    QSettings s = makeSettings(profile);

    m_hostEdit->setText(
        s.value(KEY_HOST, QStringLiteral("127.0.0.1")).toString());
    m_portSpin->setValue(
        s.value(KEY_PORT, DEFAULT_PORT).toInt());
    m_authCheck->setChecked(
        s.value(KEY_AUTH_REQUIRED, false).toBool());
    m_tokenEdit->setText(
        s.value(KEY_AUTH_TOKEN, QString()).toString());
}

bool SettingsDialog::saveDaemonConnectionSettings()
{
    QString profile = m_profileCombo->currentText();
    if (profile.isEmpty()) {
        return false;
    }

    QSettings s = makeSettings(profile);

    const QString host      = m_hostEdit->text().trimmed();
    const int     port      = m_portSpin->value();
    const bool    authReq   = m_authCheck->isChecked();
    const QString token     = m_tokenEdit->text();

    const QString oldHost   = s.value(KEY_HOST,
                                      QStringLiteral("127.0.0.1")).toString();
    const int     oldPort   = s.value(KEY_PORT, DEFAULT_PORT).toInt();
    const bool    oldAuth   = s.value(KEY_AUTH_REQUIRED, false).toBool();
    const QString oldToken  = s.value(KEY_AUTH_TOKEN, QString()).toString();

    const bool changed = (host    != oldHost)
                      || (port    != oldPort)
                      || (authReq != oldAuth)
                      || (token   != oldToken);

    if (!host.isEmpty())
    {
        s.setValue(KEY_HOST, host);
    }
    s.setValue(KEY_PORT,          port);
    s.setValue(KEY_AUTH_REQUIRED, authReq);
    s.setValue(KEY_AUTH_TOKEN,    token);
    s.sync();

    return changed;
}

QSettings SettingsDialog::makeSettings(const QString &profile)
{
    return QSettings(QSettings::IniFormat,
                     QSettings::UserScope,
                     QStringLiteral("MeagreMUD"),
                     QStringLiteral("MeagreMUD-%1").arg(profile));
}
