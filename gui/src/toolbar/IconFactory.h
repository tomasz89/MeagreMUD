#pragma once

/**
 * @file IconFactory.h
 * @brief Programmatic SVG icon factory for the MeagreMUD toolbar.
 *
 * All icons are generated from inline SVG strings - no external files or
 * Qt resource files required. Each icon is 24x24 logical pixels.
 */

#include <QIcon>

/**
 * @brief Generates QIcon instances from inline SVG for all toolbar actions.
 *
 * All methods are static. Icons are rendered at 24x24 and 48x48 (HiDPI)
 * automatically via Qt's SVG renderer.
 */
class IconFactory
{
public:
    IconFactory() = delete;

    // --- Daemon (GUI-to-daemon) connection ---

    /// Two computers with a link - open connection dialog.
    static QIcon daemonConnect();

    /// Lightning bolt plug - quick connect with last profile.
    static QIcon daemonQuickConnect();

    /// Clock with arrow - auto-connect on launch toggle.
    static QIcon daemonAutoConnect();

    /// Two screens with broken link - disconnect from daemon.
    static QIcon daemonDisconnect();

    // --- BBS (character session) connection ---

    /// Plug icon - toggles character session connection to BBS server.
    /// Use checked state for connected (green), unchecked for disconnected (grey).
    static QIcon bbsConnection();

    // --- Path / navigation ---

    /// Map pin with arrow - go to a specific location.
    static QIcon gotoLocation();

    /// Circular arrow path - run a circuit.
    static QIcon runCircuit();

    /// Stop square - cease current path or circuit.
    static QIcon cease();

    // --- Automation master ---

    /// Gear with play triangle - automation on/off master toggle.
    static QIcon automationToggle();

    // --- Automation children ---

    /// Sword crossing shield - automatic combat toggle.
    static QIcon autoCombat();

    /// Heart with plus - automatic rest/heal toggle.
    static QIcon autoRest();

    /// Star sparkle - automatic bless toggle.
    static QIcon autoBless();

    /// Hand picking up item - automatic get items toggle.
    static QIcon autoGetItems();

    /// Coin stack - automatic get money toggle.
    static QIcon autoGetMoney();

    /// Footsteps with shadow - automatic sneak toggle.
    static QIcon autoSneak();

    /// Hood/cloak - automatic hide toggle.
    static QIcon autoHide();

    /// Magnifying glass - automatic search toggle.
    static QIcon autoSearch();

    // --- Settings ---

    /// Gear/cog - open settings dialog.
    static QIcon settings();

private:
    /// Build a QIcon from an SVG string. Renders at 24x24 and 48x48.
    static QIcon fromSvg(const QString &svg);
};
