#include "toolbar/IconFactory.h"

#include <QPixmap>
#include <QPainter>
#include <QSvgRenderer>
#include <QByteArray>

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

QIcon IconFactory::fromSvg(const QString &svg)
{
    const QByteArray data = svg.toUtf8();
    QSvgRenderer renderer(data);

    QIcon icon;

    for (int size : {24, 48})
    {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        painter.end();
        icon.addPixmap(pixmap);
    }

    return icon;
}

// ---------------------------------------------------------------------------
// Daemon (GUI-to-daemon) connection icons
// ---------------------------------------------------------------------------

QIcon IconFactory::daemonConnect()
{
    // Two screens connected by a cable
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Left monitor -->
  <rect x="1" y="4" width="8" height="6" rx="1" fill="none" stroke="#3498db" stroke-width="1.2"/>
  <line x1="5" y1="10" x2="5" y2="12" stroke="#3498db" stroke-width="1.2"/>
  <line x1="3" y1="12" x2="7" y2="12" stroke="#3498db" stroke-width="1.2"/>
  <!-- Right monitor -->
  <rect x="15" y="4" width="8" height="6" rx="1" fill="none" stroke="#3498db" stroke-width="1.2"/>
  <line x1="19" y1="10" x2="19" y2="12" stroke="#3498db" stroke-width="1.2"/>
  <line x1="17" y1="12" x2="21" y2="12" stroke="#3498db" stroke-width="1.2"/>
  <!-- Connecting cable -->
  <path d="M9 7 L15 7" stroke="#2ecc71" stroke-width="1.5" stroke-dasharray="2,1"/>
  <!-- Connection dots -->
  <circle cx="9" cy="7" r="1" fill="#2ecc71"/>
  <circle cx="15" cy="7" r="1" fill="#2ecc71"/>
</svg>)svg"));
}

QIcon IconFactory::daemonQuickConnect()
{
    // Lightning bolt inside a plug - quick connect
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Plug body -->
  <rect x="2" y="10" width="9" height="4" rx="1" fill="#3498db" stroke="#2980b9" stroke-width="0.8"/>
  <!-- Prongs -->
  <rect x="4" y="7" width="2" height="3" rx="0.5" fill="#2980b9"/>
  <rect x="8" y="7" width="2" height="3" rx="0.5" fill="#2980b9"/>
  <!-- Lightning bolt -->
  <path d="M15 3 L11 13 L15 13 L11 21 L20 9 L16 9 Z"
        fill="#f39c12" stroke="#e67e22" stroke-width="0.5"/>
</svg>)svg"));
}

QIcon IconFactory::daemonAutoConnect()
{
    // Clock face with a small arrow suggesting scheduled/automatic action
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Clock face -->
  <circle cx="12" cy="12" r="9" fill="none" stroke="#7f8c8d" stroke-width="1.5"/>
  <!-- Clock hands -->
  <line x1="12" y1="12" x2="12" y2="7" stroke="#2c3e50" stroke-width="1.5" stroke-linecap="round"/>
  <line x1="12" y1="12" x2="16" y2="14" stroke="#2c3e50" stroke-width="1.5" stroke-linecap="round"/>
  <!-- Centre dot -->
  <circle cx="12" cy="12" r="1" fill="#2c3e50"/>
  <!-- Small refresh arrow in bottom-right suggesting auto -->
  <path d="M18 17 C19 15.5 19 14 18 13" stroke="#27ae60" stroke-width="1.2"
        fill="none" stroke-linecap="round"/>
  <path d="M16.5 17.5 L18 17 L18.5 15.5" stroke="#27ae60" stroke-width="1.2"
        fill="none" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)svg"));
}

QIcon IconFactory::daemonDisconnect()
{
    // Two screens with a broken/red link
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Left monitor -->
  <rect x="1" y="4" width="8" height="6" rx="1" fill="none" stroke="#7f8c8d" stroke-width="1.2"/>
  <line x1="5" y1="10" x2="5" y2="12" stroke="#7f8c8d" stroke-width="1.2"/>
  <line x1="3" y1="12" x2="7" y2="12" stroke="#7f8c8d" stroke-width="1.2"/>
  <!-- Right monitor -->
  <rect x="15" y="4" width="8" height="6" rx="1" fill="none" stroke="#7f8c8d" stroke-width="1.2"/>
  <line x1="19" y1="10" x2="19" y2="12" stroke="#7f8c8d" stroke-width="1.2"/>
  <line x1="17" y1="12" x2="21" y2="12" stroke="#7f8c8d" stroke-width="1.2"/>
  <!-- Broken cable with X -->
  <line x1="9" y1="7" x2="11" y2="7" stroke="#e74c3c" stroke-width="1.2"/>
  <line x1="13" y1="7" x2="15" y2="7" stroke="#e74c3c" stroke-width="1.2"/>
  <line x1="11" y1="5.5" x2="13" y2="8.5" stroke="#e74c3c" stroke-width="1.5" stroke-linecap="round"/>
  <line x1="13" y1="5.5" x2="11" y2="8.5" stroke="#e74c3c" stroke-width="1.5" stroke-linecap="round"/>
</svg>)svg"));
}

// ---------------------------------------------------------------------------
// BBS connection
// ---------------------------------------------------------------------------

QIcon IconFactory::bbsConnection()
{
    // Plug icon used as a toggle.
    // Qt will tint the checked state automatically via the style,
    // but we provide a single neutral plug that reads clearly both ways.
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Plug body -->
  <rect x="3" y="10" width="10" height="4" rx="1" fill="#2ecc71" stroke="#27ae60" stroke-width="0.8"/>
  <!-- Prongs -->
  <rect x="5" y="7" width="2" height="3" rx="0.5" fill="#27ae60"/>
  <rect x="9" y="7" width="2" height="3" rx="0.5" fill="#27ae60"/>
  <!-- Socket -->
  <rect x="13" y="9" width="5" height="6" rx="1" fill="none" stroke="#27ae60" stroke-width="1"/>
  <rect x="14" y="10.5" width="1.5" height="1" rx="0.2" fill="#27ae60"/>
  <rect x="16" y="10.5" width="1.5" height="1" rx="0.2" fill="#27ae60"/>
  <rect x="14" y="12.5" width="1.5" height="1" rx="0.2" fill="#27ae60"/>
  <rect x="16" y="12.5" width="1.5" height="1" rx="0.2" fill="#27ae60"/>
</svg>)svg"));
}

// ---------------------------------------------------------------------------
// Path / navigation
// ---------------------------------------------------------------------------

QIcon IconFactory::gotoLocation()
{
    // Map pin with directional arrow
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Map pin -->
  <path d="M12 2 C9.2 2 7 4.2 7 7 C7 10.5 12 17 12 17 C12 17 17 10.5 17 7 C17 4.2 14.8 2 12 2 Z"
        fill="#3498db" stroke="#2980b9" stroke-width="0.8"/>
  <circle cx="12" cy="7" r="2.5" fill="white" opacity="0.9"/>
  <!-- Arrow pointing right-down -->
  <path d="M14 18 L20 18 L20 14" stroke="#2980b9" stroke-width="1.5" fill="none" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M18 21 L20 18 L22 16" stroke="#2980b9" stroke-width="1.5" fill="none" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)svg"));
}

QIcon IconFactory::runCircuit()
{
    // Circular arrows forming a loop
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Circuit loop -->
  <path d="M12 4 C7.6 4 4 7.6 4 12 C4 16.4 7.6 20 12 20 C16.4 20 20 16.4 20 12"
        stroke="#9b59b6" stroke-width="2" fill="none" stroke-linecap="round"/>
  <!-- Arrow head at end of loop -->
  <path d="M20 12 L20 7 L15 7" stroke="#9b59b6" stroke-width="2" fill="none"
        stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M17.5 5 L20 7 L17.5 9" stroke="#9b59b6" stroke-width="2" fill="none"
        stroke-linecap="round" stroke-linejoin="round"/>
  <!-- Centre dot -->
  <circle cx="12" cy="12" r="1.5" fill="#9b59b6"/>
</svg>)svg"));
}

QIcon IconFactory::cease()
{
    // Red filled stop square
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <rect x="5" y="5" width="14" height="14" rx="2" fill="#e74c3c" stroke="#c0392b" stroke-width="0.8"/>
</svg>)svg"));
}

// ---------------------------------------------------------------------------
// Automation master
// ---------------------------------------------------------------------------

QIcon IconFactory::automationToggle()
{
    // Gear with a small play triangle inside
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Gear teeth -->
  <path d="M12 2 L13.5 5 L15.5 4 L16 6.5 L18.5 6.5 L18 9 L20 10.5 L18.5 12.5
           L20 14.5 L18 16 L18.5 18.5 L16 18.5 L15.5 21 L13.5 20 L12 23
           L10.5 20 L8.5 21 L8 18.5 L5.5 18.5 L6 16 L4 14.5 L5.5 12.5
           L4 10.5 L6 9 L5.5 6.5 L8 6.5 L8.5 4 L10.5 5 Z"
        fill="none" stroke="#7f8c8d" stroke-width="1" stroke-linejoin="round"/>
  <!-- Gear body circle -->
  <circle cx="12" cy="12" r="5" fill="#ecf0f1" stroke="#7f8c8d" stroke-width="1"/>
  <!-- Play triangle -->
  <path d="M10.5 9.5 L10.5 14.5 L15 12 Z" fill="#27ae60"/>
</svg>)svg"));
}

// ---------------------------------------------------------------------------
// Automation children
// ---------------------------------------------------------------------------

QIcon IconFactory::autoCombat()
{
    // Crossed sword and shield
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Shield -->
  <path d="M12 3 L19 6 L19 12 C19 16 12 21 12 21 C12 21 5 16 5 12 L5 6 Z"
        fill="#3498db" stroke="#2980b9" stroke-width="0.8"/>
  <!-- Sword diagonal -->
  <line x1="7" y1="7" x2="17" y2="17" stroke="white" stroke-width="2" stroke-linecap="round"/>
  <line x1="5" y1="9" x2="9" y2="5" stroke="white" stroke-width="1.5" stroke-linecap="round"/>
  <rect x="14.5" y="14.5" width="4" height="1.5" rx="0.5"
        fill="white" transform="rotate(45 16.5 15.25)"/>
</svg>)svg"));
}

QIcon IconFactory::autoRest()
{
    // Heart with a plus sign
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Heart -->
  <path d="M12 20 C12 20 3 14 3 8 C3 5.2 5.2 3 8 3 C9.8 3 11.4 4 12 5.5
           C12.6 4 14.2 3 16 3 C18.8 3 21 5.2 21 8 C21 14 12 20 12 20 Z"
        fill="#e74c3c" stroke="#c0392b" stroke-width="0.8"/>
  <!-- Plus -->
  <line x1="12" y1="7" x2="12" y2="13" stroke="white" stroke-width="2" stroke-linecap="round"/>
  <line x1="9" y1="10" x2="15" y2="10" stroke="white" stroke-width="2" stroke-linecap="round"/>
</svg>)svg"));
}

QIcon IconFactory::autoBless()
{
    // Star with sparkles
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Star -->
  <path d="M12 2 L14.4 9.2 L22 9.2 L15.8 13.8 L18.2 21 L12 16.4 L5.8 21
           L8.2 13.8 L2 9.2 L9.6 9.2 Z"
        fill="#f39c12" stroke="#e67e22" stroke-width="0.8"/>
  <!-- Sparkle lines -->
  <line x1="20" y1="3" x2="21" y2="2" stroke="#f39c12" stroke-width="1.5" stroke-linecap="round"/>
  <line x1="20" y1="2.5" x2="20" y2="4" stroke="#f39c12" stroke-width="1.5" stroke-linecap="round"/>
  <line x1="3" y1="18" x2="2" y2="19" stroke="#f39c12" stroke-width="1.5" stroke-linecap="round"/>
  <line x1="2.5" y1="17.5" x2="2.5" y2="19.5" stroke="#f39c12" stroke-width="1.5" stroke-linecap="round"/>
</svg>)svg"));
}

QIcon IconFactory::autoGetItems()
{
    // Hand reaching down to pick up an item
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Item on ground -->
  <rect x="8" y="17" width="8" height="4" rx="1" fill="#8e44ad" stroke="#6c3483" stroke-width="0.8"/>
  <rect x="10" y="15" width="4" height="2" rx="0.5" fill="#9b59b6"/>
  <!-- Hand/arm reaching down -->
  <path d="M12 2 L12 14" stroke="#7f8c8d" stroke-width="2.5" stroke-linecap="round"/>
  <path d="M9 11 C9 11 10.5 14 12 14 C13.5 14 15 11 15 11"
        stroke="#7f8c8d" stroke-width="1.5" fill="none" stroke-linecap="round"/>
  <!-- Arrow down -->
  <path d="M10.5 12.5 L12 14.5 L13.5 12.5" stroke="#7f8c8d" stroke-width="1.2"
        fill="none" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)svg"));
}

QIcon IconFactory::autoGetMoney()
{
    // Coin stack with dollar symbol
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Coin stack -->
  <ellipse cx="12" cy="19" rx="6" ry="2" fill="#f39c12" stroke="#e67e22" stroke-width="0.8"/>
  <rect x="6" y="15" width="12" height="4" fill="#f39c12"/>
  <ellipse cx="12" cy="15" rx="6" ry="2" fill="#f5b942" stroke="#e67e22" stroke-width="0.8"/>
  <rect x="6" y="11" width="12" height="4" fill="#f5b942"/>
  <ellipse cx="12" cy="11" rx="6" ry="2" fill="#f7c962" stroke="#e67e22" stroke-width="0.8"/>
  <!-- Dollar sign -->
  <text x="12" y="14" text-anchor="middle" font-size="5" font-family="sans-serif"
        font-weight="bold" fill="#7d5a00">$</text>
  <!-- Arrow down -->
  <path d="M12 2 L12 8 M10 6 L12 8 L14 6"
        stroke="#e67e22" stroke-width="1.5" fill="none" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)svg"));
}

QIcon IconFactory::autoSneak()
{
    // Footsteps with a shadow/opacity effect
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Left footprint -->
  <ellipse cx="8" cy="15" rx="2.5" ry="4" fill="#7f8c8d" opacity="0.4" transform="rotate(-15 8 15)"/>
  <ellipse cx="8" cy="11.5" rx="1" ry="1.2" fill="#7f8c8d" opacity="0.4" transform="rotate(-15 8 11.5)"/>
  <!-- Right footprint -->
  <ellipse cx="15" cy="9" rx="2.5" ry="4" fill="#2c3e50" opacity="0.7" transform="rotate(15 15 9)"/>
  <ellipse cx="14.5" cy="5.5" rx="1" ry="1.2" fill="#2c3e50" opacity="0.7" transform="rotate(15 14.5 5.5)"/>
  <!-- Speed lines suggesting quiet movement -->
  <line x1="3" y1="12" x2="6" y2="12" stroke="#7f8c8d" stroke-width="0.8" stroke-dasharray="1,1" opacity="0.5"/>
  <line x1="3" y1="14" x2="5" y2="14" stroke="#7f8c8d" stroke-width="0.8" stroke-dasharray="1,1" opacity="0.4"/>
</svg>)svg"));
}

QIcon IconFactory::autoHide()
{
    // Hood/cloak shape
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Hood outline -->
  <path d="M12 2 C7 2 4 6 4 10 L4 20 L8 20 L8 14 C8 12 10 11 12 11
           C14 11 16 12 16 14 L16 20 L20 20 L20 10 C20 6 17 2 12 2 Z"
        fill="#2c3e50" stroke="#1a252f" stroke-width="0.8"/>
  <!-- Face shadow (hidden face) -->
  <ellipse cx="12" cy="9" rx="3" ry="3.5" fill="#1a252f" opacity="0.6"/>
  <!-- Eye slits -->
  <line x1="10" y1="8.5" x2="11" y2="8.5" stroke="#ecf0f1" stroke-width="0.8" stroke-linecap="round"/>
  <line x1="13" y1="8.5" x2="14" y2="8.5" stroke="#ecf0f1" stroke-width="0.8" stroke-linecap="round"/>
</svg>)svg"));
}

QIcon IconFactory::autoSearch()
{
    // Magnifying glass
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Glass circle -->
  <circle cx="10" cy="10" r="6.5" fill="none" stroke="#3498db" stroke-width="2"/>
  <!-- Handle -->
  <line x1="15" y1="15" x2="21" y2="21" stroke="#3498db" stroke-width="2.5" stroke-linecap="round"/>
  <!-- Inner highlight -->
  <circle cx="8" cy="8" r="2" fill="none" stroke="#7ec8e3" stroke-width="1" opacity="0.6"/>
</svg>)svg"));
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

QIcon IconFactory::settings()
{
    // Classic gear/cog
    return fromSvg(QStringLiteral(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <!-- Outer gear ring with teeth -->
  <path d="M12 1 L14 4.5 L17.5 3 L18.5 6.5 L22 7 L21 10.5 L23 12
           L21 13.5 L22 17 L18.5 17.5 L17.5 21 L14 19.5 L12 23
           L10 19.5 L6.5 21 L5.5 17.5 L2 17 L3 13.5 L1 12
           L3 10.5 L2 7 L5.5 6.5 L6.5 3 L10 4.5 Z"
        fill="#7f8c8d" stroke="#6c7a7d" stroke-width="0.5" stroke-linejoin="round"/>
  <!-- Inner circle -->
  <circle cx="12" cy="12" r="4.5" fill="#ecf0f1" stroke="#bdc3c7" stroke-width="0.8"/>
  <!-- Centre dot -->
  <circle cx="12" cy="12" r="1.5" fill="#7f8c8d"/>
</svg>)svg"));
}
