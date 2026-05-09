#pragma once

/**
 * @file ConnectionDialog.h
 * @brief Dedicated dialog for connecting to the daemon with profile management.
 */

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

/**
 * @brief Dialog for selecting a daemon connection profile and connecting.
 *
 * Allows users to select from existing profiles or create new ones,
 * edit connection settings, and initiate connection/disconnection.
 */
class ConnectionDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Construct the connection dialog.
     * @param parent Qt parent widget.
     */
    explicit ConnectionDialog(QWidget *parent = nullptr);

    ~ConnectionDialog() override;

signals:
    /**
     * @brief Emitted when the user requests to connect with a profile.
     * @param profile The selected profile name.
     */
    void connectRequested(const QString &profile);

    /**
     * @brief Emitted when the user requests to disconnect.
     */
    void disconnectRequested();

private slots:
    void onProfileChanged();
    void onAddProfile();
    void onRemoveProfile();
    void onSaveProfile();
    void onConnect();
    void onDisconnect();

private:
    void loadProfiles();
    void saveProfiles();
    void loadProfileSettings(const QString &profile);
    void saveProfileSettings(const QString &profile);
    void removeProfileSettings(const QString &profile);
    bool currentSettingsMatchSaved() const;
    void updateButtonStates();
    static QSettings makeSettings();

    QComboBox *m_profileCombo = nullptr;
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QLineEdit  *m_hostEdit     = nullptr;
    QSpinBox  *m_portSpin     = nullptr;
    QCheckBox *m_authCheck    = nullptr;
    QLineEdit  *m_tokenEdit    = nullptr;
    QLabel    *m_tokenLabel   = nullptr;
    QDialogButtonBox *m_buttons = nullptr;

    QStringList m_profiles;

    static constexpr int DEFAULT_PORT = 7777;
    static constexpr const char *KEY_HOST          = "host";
    static constexpr const char *KEY_PORT          = "port";
    static constexpr const char *KEY_AUTH_REQUIRED = "auth_required";
    static constexpr const char *KEY_AUTH_TOKEN    = "auth_token";
    static constexpr const char *KEY_PROFILES      = "profiles/list";
};
