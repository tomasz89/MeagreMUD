#pragma once

#include <QtGlobal>
#include <QString>
#include <QVector>

/**
 * @file MudTypes.h
 * @brief Shared value types used across the daemon, GUI, and scraper.
 */

/// Palette index value indicating the terminal default colour (fg or bg).
static constexpr quint8 COLOUR_DEFAULT = 0xFF;

/**
 * @brief A span of text rendered with a single consistent style.
 *
 * AnsiParser emits one StyledRun each time an ANSI SGR escape sequence
 * completes, bundling all text accumulated since the previous style change.
 *
 * @note @c fg and @c bg are indices into the 16-colour ANSI palette
 *       (0 - 7 normal, 8 - 15 bright). COLOUR_DEFAULT (0xFF) means "use the
 *       terminal default colour".
 *
 * Wire format (daemon -> GUI):
 * @code
 * [fg: 1][bg: 1][bold: 1][text_len: 2][text: N (UTF-8)]
 * @endcode
 */
struct StyledRun {
    quint8  fg   = COLOUR_DEFAULT; ///< Foreground palette index or COLOUR_DEFAULT.
    quint8  bg   = COLOUR_DEFAULT; ///< Background palette index or COLOUR_DEFAULT.
    bool    bold = false;          ///< Whether bold attribute is set.
    QString text;                  ///< UTF-8 text content. May contain \\n and \\r\\n.
};

/**
 * @brief A vector of StyledRuns forming one display line.
 *
 * The last Line in the backbuffer is always open (in-progress). Lines are
 * closed only when \\n is encountered within a run's text by TerminalWidget.
 *
 * @see TerminalWidget
 */
using Line = QVector<StyledRun>;
