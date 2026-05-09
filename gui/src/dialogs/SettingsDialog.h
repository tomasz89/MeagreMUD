#pragma once

/**
 * @file SettingsDialog.h
 * @brief Application settings dialog with six tabs.
 */

#include <QDialog>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QSettings>

/**
 * @brief Application settings dialog.
 *
 * Single QDialog with six tabs and OK / Cancel / Apply buttons. Tab 6
 * (Daemon Connection) is fully implemented and backed by QSettings INI.
 * Tabs 1–5 are stubs pending SQLite-backed implementation.
 *
 * ## Tab summary
 * | # | Name | Status | Backend |
 * |---|------|--------|---------|
 * | 1 | Servers & Characters | Stub | SQLite |
 * | 2 | Display | Stub | SQLite |
 * | 3 | Key Macros | Stub | SQLite |
 * | 4 | Triggers & Automation | Stub | SQLite |
 * | 5 | Server Quirks | Stub | SQLite |
 * | 6 | Daemon Connection | **Implemented** | QSettings INI |
 *
 * Tab 6 stores only what is needed to reach the daemon: host, port, and
 * an optional authentication token. All other configuration is managed via
 * DatabaseManager by the daemon.
 *
 * ## Usage
 * @code
 * SettingsDialog dlg(this);
 * connect(&dlg, &SettingsDialog::daemonConnectionChanged,
 *         this, &MyClass::onDaemonSettingsChanged);
 * dlg.exec();
 * @endcode
 *
 * To open directly to a specific tab:
 * @code
 * dlg.setCurrentTab(SettingsDialog::Tab::DaemonConnection);
 * dlg.exec();
 * @endcode
 *
 * Saved settings can be read without opening the dialog:
 * @code
 * QString host = SettingsDialog::savedHost();
 * quint16 port = SettingsDialog::savedPort();
 * @endcode
 *
 * @see DaemonConnectionManager, MainWindow
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Identifies each tab by logical name rather than index.
     */
    enum class Tab {
        ServersAndCharacters  = 0, ///< Server and character configuration (stub).
        Display               = 1, ///< Display and palette settings (stub).
        KeyMacros             = 2, ///< Key macro bindings (stub).
        TriggersAndAutomation = 3, ///< Trigger groups and automation (stub).
        ServerQuirks          = 4, ///< Per-server quirk key/value table (stub).
        DaemonConnection      = 5, ///< Daemon host, port, and auth (implemented).
    };

    /**
     * @brief Construct the dialog.
     * @param parent Qt parent widget.
     */
    explicit SettingsDialog(QWidget *parent = nullptr);

    /**
     * @brief Switch to a specific tab programmatically.
     * @param tab  Tab to show.
     */
    void setCurrentTab(Tab tab);

    /**
     * @brief Read the saved daemon host without opening the dialog.
     * @param profile The profile name.
     * @return Saved host, or @c "127.0.0.1" if not yet configured.
     */
    static QString savedHost(const QString &profile = QStringLiteral("Default"));

    /**
     * @brief Read the saved daemon port without opening the dialog.
     * @param profile The profile name.
     * @return Saved port, or 7777 if not yet configured.
     */
    static quint16 savedPort(const QString &profile = QStringLiteral("Default"));

    /**
     * @brief Read the saved auth token.
     * @param profile The profile name.
     * @return Saved token, or empty string.
     */
    static QString savedAuthToken(const QString &profile = QStringLiteral("Default"));

    /**
     * @brief Read whether auth is required.
     * @param profile The profile name.
     * @return True if auth is required.
     */
    static bool savedAuthRequired(const QString &profile = QStringLiteral("Default"));

signals:
    /**
     * @brief Emitted when daemon connection settings are saved (OK or Apply).
     *
     * Only emitted if at least one value actually changed. MainWindow
     * connects to this to notify the user that a reconnect may be needed.
     */
    void daemonConnectionChanged();

private slots:
    void onAccepted();
    void onApply();

    void onProfileChanged();
    void onAddProfile();
    void onRemoveProfile();

private:
    QWidget *buildServersAndCharactersTab();
    QWidget *buildDisplayTab();
    QWidget *buildKeyMacrosTab();
    QWidget *buildTriggersTab();
    QWidget *buildServerQuirksTab();
    QWidget *buildDaemonConnectionTab();

    void loadDaemonConnectionSettings();
    bool saveDaemonConnectionSettings();

    void loadProfiles();
    void saveProfiles();
    QStringList profiles() const;
    void setProfiles(const QStringList &profiles);

    static QSettings makeSettings(const QString &profile = QStringLiteral("Default"));

    QTabWidget       *m_tabs    = nullptr; ///< The six-tab widget.
    QDialogButtonBox *m_buttons = nullptr; ///< OK / Cancel / Apply buttons.

    QStringList       m_profiles;          ///< List of available profiles.

    // Tab 6 widgets
    QComboBox *m_profileCombo = nullptr; ///< Profile selector.
    QLineEdit *m_hostEdit   = nullptr; ///< Daemon hostname/IP input.
    QSpinBox  *m_portSpin   = nullptr; ///< Daemon port spinbox.
    QCheckBox *m_authCheck  = nullptr; ///< "Require authentication" checkbox.
    QLineEdit *m_tokenEdit  = nullptr; ///< Auth token input (password echo).
    QLabel    *m_tokenLabel = nullptr; ///< Label for the token field.

    /// @name QSettings keys for Tab 6
    /// @{
    static constexpr const char *KEY_HOST          = "daemon/host";          ///< Daemon hostname.
    static constexpr const char *KEY_PORT          = "daemon/port";          ///< Daemon port.
    static constexpr const char *KEY_AUTH_REQUIRED = "daemon/auth_required"; ///< Auth flag.
    static constexpr const char *KEY_AUTH_TOKEN    = "daemon/auth_token";    ///< Auth token.
    /// @}

    static constexpr quint16 DEFAULT_PORT = 7777; ///< Default port when nothing is saved.
};
