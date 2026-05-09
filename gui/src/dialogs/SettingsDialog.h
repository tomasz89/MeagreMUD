#pragma once

/**
 * @file SettingsDialog.h
 * @brief Application settings dialog with five tabs.
 */

#include <QDialog>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSettings>

/**
 * @brief Application settings dialog.
 *
 * Single QDialog with five tabs and OK / Cancel / Apply buttons.
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
 *
 * Tabs 1–5 are stubs pending SQLite-backed implementation.
 *
 * ## Usage
 * @code
 * SettingsDialog dlg(this);
 * dlg.exec();
 * @endcode
 *
 * Saved daemon connection settings can still be read without opening the dialog:
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

private slots:
    void onAccepted();
    void onApply();

private:
    QWidget *buildServersAndCharactersTab();
    QWidget *buildDisplayTab();
    QWidget *buildKeyMacrosTab();
    QWidget *buildTriggersTab();
    QWidget *buildServerQuirksTab();

    static QSettings makeSettings(const QString &profile = QStringLiteral("Default"));

    QTabWidget       *m_tabs    = nullptr; ///< The tab widget.
    QDialogButtonBox *m_buttons = nullptr; ///< OK / Cancel / Apply buttons.

    /// @name QSettings keys for saved daemon connection settings
    /// {@
    static constexpr const char *KEY_HOST          = "daemon/host";          ///< Daemon hostname.
    static constexpr const char *KEY_PORT          = "daemon/port";          ///< Daemon port.
    static constexpr const char *KEY_AUTH_REQUIRED = "daemon/auth_required"; ///< Auth flag.
    static constexpr const char *KEY_AUTH_TOKEN    = "daemon/auth_token";    ///< Auth token.
    /// @}

    static constexpr quint16 DEFAULT_PORT = 7777; ///< Default port when nothing is saved.
};
