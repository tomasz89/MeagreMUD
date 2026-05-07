#pragma once

#include <QWidget>
#include <QFont>
#include <QColor>
#include <QVector>
#include <QScrollBar>

#include "types/MudTypes.h"

// ---------------------------------------------------------------------------
// TerminalWidget
//
// Custom terminal display widget. Renders a circular backbuffer of Lines,
// each a QVector<StyledRun>. No QTextEdit, no HTML — pure paintEvent.
//
// Backbuffer rules:
//   - Capacity is set at construction and is fixed for the widget's lifetime.
//     Changes require re-creating the widget (capacity is a CharacterConfig
//     concern resolved before the pane is built).
//   - The last Line in the backbuffer is always open/in-progress. It is
//     rendered immediately as runs arrive and closed only when \n appears.
//   - \n within a run's text closes the current open line and opens a new one.
//     \n is never used as a flush trigger — runs arrive already flushed by
//     AnsiParser on escape-sequence completion.
//   - \r without a following \n overwrites the open line (classic CR behaviour).
//
// Scrollback:
//   - User can scroll back through the entire backbuffer using the mouse wheel
//     or the vertical scrollbar.
//   - Scrollback is reset to the bottom when a new run arrives (live follow),
//     unless the user is actively scrolled up.
//   - "Actively scrolled up" = scrollbar is not at maximum.
//
// Palette:
//   - 16-colour ANSI palette (indices 0-7 normal, 8-15 bright).
//   - Default palette is a standard dark terminal: black background,
//     light grey foreground. Configurable via setColour().
//   - fg/bg 0xFF = default terminal colour (m_defaultFg / m_defaultBg).
//
// Font:
//   - Monospaced. Set via setTerminalFont(). Line height and cell width
//     are derived from QFontMetrics on each font change.
//   - The widget does not enforce that the font is monospaced — that is
//     the user's responsibility via the settings dialog.
// ---------------------------------------------------------------------------

class TerminalWidget : public QWidget
{
    Q_OBJECT

public:
    static constexpr int DEFAULT_BACKBUFFER_LINES = 1000;
    static constexpr int PALETTE_SIZE             = 16;

    explicit TerminalWidget(int backbufferCapacity = DEFAULT_BACKBUFFER_LINES,
                            QWidget *parent = nullptr);

    // Append a StyledRun to the open line. Handles \n and \r within the
    // run's text, splitting or overwriting lines as required.
    void appendRun(const StyledRun &run);

    // Clear the entire backbuffer and open a fresh line.
    // Called on MSG_SCREEN_CLEARED (ESC[2J from daemon).
    void clearScreen();

    // Font — must be monospaced. Triggers a full repaint.
    void setTerminalFont(const QFont &font);
    QFont terminalFont() const { return m_font; }

    // Palette — index 0-15 for ANSI colours, or use setDefaultColours().
    void setColour(int index, const QColor &colour);
    void setDefaultForeground(const QColor &colour);
    void setDefaultBackground(const QColor &colour);

    QColor colour(int index) const;
    QColor defaultForeground() const { return m_defaultFg; }
    QColor defaultBackground() const { return m_defaultBg; }

    int backbufferCapacity() const { return m_capacity; }

    // Number of complete lines currently in the backbuffer (excludes the
    // open line).
    int lineCount() const;

protected:
    void paintEvent(QPaintEvent *event)     override;
    void resizeEvent(QResizeEvent *event)   override;
    void wheelEvent(QWheelEvent *event)     override;
    void keyPressEvent(QKeyEvent *event)    override;

private:
    // Resolve a fg or bg palette index to a QColor.
    // 0xFF = default colour.
    QColor resolveFg(quint8 index) const;
    QColor resolveBg(quint8 index) const;

    // Re-derive cell metrics from current font and widget width.
    void updateMetrics();

    // Update scrollbar range and page step to match current backbuffer and
    // visible line count. Does not move the scrollbar position.
    void updateScrollbar();

    // Scroll the view to the bottom (most recent output).
    void scrollToBottom();

    // True if the scrollbar is at maximum (user is following live output).
    bool isAtBottom() const;

    // Number of complete lines visible in the widget at current height.
    int visibleLineCount() const;

    // Split a run's text on \n boundaries, dispatching each segment.
    // Handles \r within segments.
    void processRunText(const StyledRun &run);

    // Append a single segment (no embedded \n) to the open line.
    void appendSegmentToOpenLine(const StyledRun &run, const QString &text);

    // Close the current open line and open a new empty one.
    void commitOpenLine();

    // Overwrite the open line with a carriage-return (discard its content).
    void carriageReturn();

    // Access helpers into the circular buffer.
    // Index 0 = oldest line, index (m_lineCount-1) = open line.
    Line       &lineAt(int logicalIndex);
    const Line &lineAt(int logicalIndex) const;

    // Circular buffer storage
    QVector<Line>   m_buffer;       // fixed size = m_capacity
    int             m_capacity;     // max lines (including open line)
    int             m_head;         // index of the oldest line in m_buffer
    int             m_lineCount;    // number of lines currently stored
                                    // (starts at 1 for the open line)

    // Scrollback
    QScrollBar     *m_scrollbar;
    int             m_scrollOffset; // 0 = at bottom; N = N lines scrolled up

    // Font and cell metrics
    QFont           m_font;
    int             m_lineHeight;   // pixels per line (ascent + descent + leading)
    int             m_charWidth;    // pixels per character cell (monospaced)
    int             m_ascent;       // font ascent — used for text baseline

    // Palette
    QColor          m_palette[PALETTE_SIZE];
    QColor          m_defaultFg;
    QColor          m_defaultBg;

    // Pending CR flag — set when \r is seen without an immediately following
    // \n in the same run. Cleared on next character.
    bool            m_pendingCr;
};
