#include "dialogs/SettingsDialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

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
    QSettings s = makeSettings();
    s.beginGroup(QStringLiteral("profile-%1").arg(profile));
    QString host = s.value(KEY_HOST, QStringLiteral("127.0.0.1")).toString();
    s.endGroup();
    return host;
}

quint16 SettingsDialog::savedPort(const QString &profile)
{
    QSettings s = makeSettings();
    s.beginGroup(QStringLiteral("profile-%1").arg(profile));
    quint16 port = static_cast<quint16>(
        s.value(KEY_PORT, DEFAULT_PORT).toUInt());
    s.endGroup();
    return port;
}

QString SettingsDialog::savedAuthToken(const QString &profile)
{
    QSettings s = makeSettings();
    s.beginGroup(QStringLiteral("profile-%1").arg(profile));
    QString authToken = s.value(KEY_AUTH_TOKEN, QString()).toString();
    s.endGroup();
    return authToken;
}

bool SettingsDialog::savedAuthRequired(const QString &profile)
{
    QSettings s = makeSettings();
    s.beginGroup(QStringLiteral("profile-%1").arg(profile));
    bool savedAuthRequired = s.value(KEY_AUTH_REQUIRED, false).toBool();
    s.endGroup();
    return savedAuthRequired;
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
    Q_UNUSED(this);
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
// Settings helpers
// ---------------------------------------------------------------------------

QSettings SettingsDialog::makeSettings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("MeagreMUD"),
                     QStringLiteral("MeagreMUD"));
}
