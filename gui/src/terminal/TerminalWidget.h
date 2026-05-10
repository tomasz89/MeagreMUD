#pragma once

/**
 * @file TerminalWidget.h
 * @brief Custom MUD terminal display widget using a circular backbuffer.
 */

#include <QWidget>
#include <QFont>
#include <QColor>
#include <QVector>
#include <QScrollBar>

#include "types/MudTypes.h"

/**
 * @brief Custom terminal display widget rendering a circular backbuffer of
 *        StyledRun lines. Uses a pure paintEvent  -  no QTextEdit, no HTML.
 *
 * ## Backbuffer rules
 * - Capacity is fixed at construction time. Changing it requires re-creating
 *   the widget (capacity is resolved from CharacterConfig before the pane
 *   is built).
 * - The last Line in the backbuffer is always open (in-progress). It is
 *   rendered immediately as runs arrive and closed only when @c \\n appears
 *   within a run's text.
 * - @c \\n closes the current open line and opens a new one.
 *   @c \\n is never a flush trigger  -  runs arrive pre-flushed by AnsiParser.
 * - @c \\r without a following @c \\n overwrites (clears) the open line.
 *
 * ## Scrollback
 * - The user can scroll back through the entire backbuffer via the mouse
 *   wheel or the vertical scrollbar.
 * - Scrollback snaps to the bottom when a new run arrives, unless the user
 *   has actively scrolled up (scrollbar not at maximum).
 *
 * ## Palette
 * - 16-colour ANSI palette: indices 0 - 7 normal, 8 - 15 bright.
 * - Default: black background, light grey foreground.
 * - COLOUR_DEFAULT (0xFF) maps to the configured default fg or bg colour.
 *
 * ## Font
 * - Must be monospaced. Set via setTerminalFont(). Line height and cell
 *   width are re-derived from QFontMetrics on each change.
 *
 * @see StyledRun, Line, AnsiParser
 */
class TerminalWidget : public QWidget
{
    Q_OBJECT

public:
    /// Default backbuffer capacity in lines (including the open line).
    static constexpr int DEFAULT_BACKBUFFER_LINES = 1000;

    /// Number of colours in the ANSI palette (8 normal + 8 bright).
    static constexpr int PALETTE_SIZE = 16;

    /**
     * @brief Construct the widget.
     * @param backbufferCapacity  Maximum number of lines to retain
     *                            (including the always-open last line).
     *                            Clamped to a minimum of 2.
     * @param parent              Qt parent widget.
     */
    explicit TerminalWidget(int backbufferCapacity = DEFAULT_BACKBUFFER_LINES,
                            QWidget *parent = nullptr);

    /**
     * @brief Append a StyledRun to the currently open line.
     *
     * Handles @c \\n and @c \\r within the run's text, splitting or
     * overwriting lines as required. Scrolls to bottom if the user was
     * already following live output.
     *
     * @param run  The styled text run to append.
     */
    void appendRun(const StyledRun &run);

    /**
     * @brief Clear the entire backbuffer and open a fresh empty line.
     *
     * Called when ESC[2J (screen clear) is received from the daemon.
     */
    void clearScreen();

    /**
     * @brief Set the display font. Should be monospaced.
     *
     * Recomputes line height and character cell width from QFontMetrics
     * and triggers a full repaint.
     *
     * @param font  Font to use. Not validated as monospaced.
     */
    void setTerminalFont(const QFont &font);

    /// @return The currently active terminal font.
    QFont terminalFont() const { return m_font; }

    /**
     * @brief Set a palette colour by index.
     * @param index   Palette index 0 - 15.
     * @param colour  Colour to assign.
     */
    void setColour(int index, const QColor &colour);

    /**
     * @brief Set the default foreground colour (used when fg = COLOUR_DEFAULT).
     * @param colour  Foreground colour.
     */
    void setDefaultForeground(const QColor &colour);

    /**
     * @brief Set the default background colour (used when bg = COLOUR_DEFAULT).
     * @param colour  Background colour.
     */
    void setDefaultBackground(const QColor &colour);

    /**
     * @brief Get a palette colour by index.
     * @param index  Palette index 0 - 15.
     * @return       The colour at that index, or defaultForeground() if out of range.
     */
    QColor colour(int index) const;

    /// @return The current default foreground colour.
    QColor defaultForeground() const { return m_defaultFg; }

    /// @return The current default background colour.
    QColor defaultBackground() const { return m_defaultBg; }

    /// @return The maximum number of lines the backbuffer can hold.
    int backbufferCapacity() const { return m_capacity; }

    /**
     * @brief Number of complete (closed) lines in the backbuffer.
     *
     * Does not count the always-open last line.
     *
     * @return Number of complete historical lines.
     */
    int lineCount() const;

protected:
    /// @cond INTERNAL
    void paintEvent(QPaintEvent *event)   override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event)   override;
    void keyPressEvent(QKeyEvent *event)  override;
    /// @endcond

private:
    QColor resolveFg(quint8 index) const;
    QColor resolveBg(quint8 index) const;
    void updateMetrics();
    void updateScrollbar();
    void scrollToBottom();
    bool isAtBottom() const;
    int  visibleLineCount() const;
    void processRunText(const StyledRun &run);
    void appendSegmentToOpenLine(const StyledRun &run, const QString &text);
    void commitOpenLine();
    void carriageReturn();
    Line       &lineAt(int logicalIndex);
    const Line &lineAt(int logicalIndex) const;

    QVector<Line> m_buffer;       ///< Circular line buffer; fixed size = m_capacity.
    int           m_capacity;     ///< Maximum lines (including open line).
    int           m_head;         ///< Physical index of the oldest logical line.
    int           m_lineCount;    ///< Number of lines currently stored (min 1).

    QScrollBar   *m_scrollbar;
    int           m_scrollOffset; ///< 0 = following bottom; N = N lines scrolled up.

    QFont         m_font;
    int           m_lineHeight;   ///< Pixels per line (ascent + descent + leading).
    int           m_charWidth;    ///< Pixels per character cell (monospaced assumption).
    int           m_ascent;       ///< Font ascent used for drawText baseline.

    QColor        m_palette[PALETTE_SIZE];
    QColor        m_defaultFg;
    QColor        m_defaultBg;

    bool          m_pendingCr;    ///< Set on bare \\r; resolved on next char.
};
