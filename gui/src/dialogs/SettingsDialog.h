#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QSettings>

// ---------------------------------------------------------------------------
// SettingsDialog
//
// Single QDialog, six tabs, OK / Cancel / Apply.
//
// Tab 1 — Servers & Characters   (stub — backed by SQLite)
// Tab 2 — Display                (stub — backed by SQLite)
// Tab 3 — Key Macros             (stub — backed by SQLite)
// Tab 4 — Triggers & Automation  (stub — backed by SQLite)
// Tab 5 — Server Quirks          (stub — backed by SQLite)
// Tab 6 — Daemon Connection      (FULLY IMPLEMENTED — backed by QSettings INI)
//
// Tab 6 stores only what is needed to reach the daemon:
//   host, port, auth_token (optional).
// All other configuration goes through SQLite via DatabaseManager.
//
// Usage:
//   SettingsDialog dlg(this);
//   if (dlg.exec() == QDialog::Accepted) { ... }
//
// Or open directly to a specific tab:
//   dlg.setCurrentTab(SettingsDialog::Tab::DaemonConnection);
//   dlg.exec();
// ---------------------------------------------------------------------------

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Tab {
        ServersAndCharacters = 0,
        Display              = 1,
        KeyMacros            = 2,
        TriggersAndAutomation = 3,
        ServerQuirks         = 4,
        DaemonConnection     = 5,
    };

    explicit SettingsDialog(QWidget *parent = nullptr);

    void setCurrentTab(Tab tab);

    // Read the stored daemon connection settings without opening the dialog.
    // Returns defaults if nothing has been saved yet.
    static QString savedHost();
    static quint16 savedPort();
    static QString savedAuthToken();
    static bool    savedAuthRequired();

signals:
    // Emitted when the user accepts and daemon connection settings have changed.
    void daemonConnectionChanged();

private slots:
    void onAccepted();
    void onApply();

private:
    // Tab builders
    QWidget *buildServersAndCharactersTab();
    QWidget *buildDisplayTab();
    QWidget *buildKeyMacrosTab();
    QWidget *buildTriggersTab();
    QWidget *buildServerQuirksTab();
    QWidget *buildDaemonConnectionTab();

    // Populate Tab 6 widgets from QSettings
    void loadDaemonConnectionSettings();

    // Persist Tab 6 widgets to QSettings. Returns true if any value changed.
    bool saveDaemonConnectionSettings();

    // Helpers
    static QSettings makeSettings();

    QTabWidget        *m_tabs       = nullptr;
    QDialogButtonBox  *m_buttons    = nullptr;

    // Tab 6 widgets
    QLineEdit  *m_hostEdit      = nullptr;
    QSpinBox   *m_portSpin      = nullptr;
    QCheckBox  *m_authCheck     = nullptr;
    QLineEdit  *m_tokenEdit     = nullptr;
    QLabel     *m_tokenLabel    = nullptr;

    // QSettings keys
    static constexpr const char *KEY_HOST         = "daemon/host";
    static constexpr const char *KEY_PORT         = "daemon/port";
    static constexpr const char *KEY_AUTH_REQUIRED = "daemon/auth_required";
    static constexpr const char *KEY_AUTH_TOKEN   = "daemon/auth_token";


    static constexpr quint16     DEFAULT_PORT      = 7777;
};
