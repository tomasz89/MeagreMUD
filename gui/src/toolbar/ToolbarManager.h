#pragma once

/**
 * @file ToolbarManager.h
 * @brief Manages the MeagreMUD customisable main toolbar.
 *
 * The toolbar has two phases:
 *   Pre-connection  -- only the daemon connect/disconnect action is shown.
 *   Post-connection -- full action set shown/hidden per user preference.
 *
 * Note: "connect/disconnect" here refers to the CHARACTER SESSION connection
 * to the BBS/MUD server, not the GUI-to-daemon TCP connection.
 *
 * Toolbar customisation (which buttons are visible and their order) is
 * persisted in the DisplaySettings database table (owner_type=0, global)
 * under the key "toolbar_buttons" as a comma-separated list of button IDs.
 *
 * The automation toggle group:
 *   The master automation toggle (automationToggle) disables all child
 *   automation actions when turned off, but does NOT uncheck them.
 *   Their checked state is preserved so re-enabling restores previous state.
 */

#include <QObject>
#include <QToolBar>
#include <QAction>
#include <QActionGroup>
#include <QMap>
#include <QStringList>

class MainWindow;

/**
 * @brief Owns and manages the main application toolbar.
 *
 * Created by MainWindow. Exposes the toolbar widget for insertion into the
 * window layout. Responds to connection state changes to show/hide the
 * appropriate action set.
 */
class ToolbarManager : public QObject
{
    Q_OBJECT

public:
    /// Stable string identifiers for each toolbar button.
    /// Used for serialisation in DisplaySettings.
    static constexpr const char *ID_BBS_CONNECT      = "bbs_connect";
    static constexpr const char *ID_BBS_DISCONNECT    = "bbs_disconnect";
    static constexpr const char *ID_GOTO_LOCATION     = "goto_location";
    static constexpr const char *ID_RUN_CIRCUIT       = "run_circuit";
    static constexpr const char *ID_CEASE             = "cease";
    static constexpr const char *ID_AUTOMATION        = "automation";
    static constexpr const char *ID_AUTO_COMBAT       = "auto_combat";
    static constexpr const char *ID_AUTO_REST         = "auto_rest";
    static constexpr const char *ID_AUTO_BLESS        = "auto_bless";
    static constexpr const char *ID_AUTO_GET_ITEMS    = "auto_get_items";
    static constexpr const char *ID_AUTO_GET_MONEY    = "auto_get_money";
    static constexpr const char *ID_AUTO_SNEAK        = "auto_sneak";
    static constexpr const char *ID_AUTO_HIDE         = "auto_hide";
    static constexpr const char *ID_AUTO_SEARCH       = "auto_search";
    static constexpr const char *ID_SETTINGS          = "settings";

    /// Default button order (used when no DB preference is saved yet).
    static QStringList defaultButtonOrder();

    /**
     * @brief Construct the toolbar manager.
     * @param mainWindow  Parent MainWindow. Used for signal routing only.
     * @param parent      Qt parent object.
     */
    explicit ToolbarManager(MainWindow *mainWindow, QObject *parent = nullptr);

    /// @return The managed QToolBar. Add to MainWindow via addToolBar().
    QToolBar *toolbar() const { return m_toolbar; }

    /// @return The action for the given button ID, or nullptr if unknown.
    QAction *action(const QString &id) const;

    /**
     * @brief Apply the pre-connection toolbar state.
     *
     * Hides all actions except the daemon connect toggle and its separator.
     * Called when the GUI-daemon connection is not yet established.
     */
    void applyPreConnectionState();

    /**
     * @brief Apply the post-connection toolbar state.
     *
     * Shows actions according to the saved button order from DisplaySettings.
     * Called once the GUI has completed resync with the daemon.
     *
     * @param buttonOrder  Ordered list of button IDs to show.
     *                     Separator positions are encoded as "separator".
     */
    void applyPostConnectionState(const QStringList &buttonOrder);

    /**
     * @brief Update the BBS connect/disconnect action state.
     *
     * Called when the active character's BBS connection status changes.
     *
     * @param connected  true if the character is connected to the BBS.
     */
    void setBbsConnected(bool connected);

    /**
     * @brief Enable or disable all automation child actions.
     *
     * Does not change their checked state.
     *
     * @param enabled  true to enable child automation actions.
     */
    void setAutomationChildrenEnabled(bool enabled);

signals:
    /// Emitted when the user clicks BBS Connect.
    void bbsConnectRequested();

    /// Emitted when the user clicks BBS Disconnect.
    void bbsDisconnectRequested();

    /// Emitted when the user clicks Go To Location.
    void gotoLocationRequested();

    /// Emitted when the user clicks Run Circuit.
    void runCircuitRequested();

    /// Emitted when the user clicks Cease.
    void ceaseRequested();

    /// Emitted when the automation master toggle changes.
    /// @param enabled true = automation on.
    void automationToggled(bool enabled);

    /// Emitted when any child automation toggle changes.
    /// @param id      Button ID of the changed toggle.
    /// @param enabled New checked state.
    void automationChildToggled(const QString &id, bool enabled);

    /// Emitted when the user clicks Settings.
    void settingsRequested();

private slots:
    void onAutomationToggled(bool checked);
    void onChildToggled(bool checked);

private:
    void createActions();
    void createToolbar();
    void buildDefaultToolbar();

    QToolBar *m_toolbar = nullptr;
    MainWindow *m_mainWindow = nullptr;

    // All managed actions keyed by ID
    QMap<QString, QAction *> m_actions;

    // Separators (dynamically added/removed on state change)
    QAction *m_preSeparator  = nullptr; // between connect and rest of toolbar
    QAction *m_autoSeparator = nullptr; // before settings button

    // Child automation actions for bulk enable/disable
    QList<QAction *> m_automationChildren;
};
