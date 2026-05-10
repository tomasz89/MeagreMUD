#include "terminal/TerminalWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QFontMetrics>
#include <QVBoxLayout>
#include <QHBoxLayout>

// ---------------------------------------------------------------------------
// Default ANSI palette  -  standard dark terminal colours.
//
// Indices 0-7:  normal colours
// Indices 8-15: bright/bold variants
// ---------------------------------------------------------------------------

static const QColor DEFAULT_PALETTE[TerminalWidget::PALETTE_SIZE] = {
    QColor(  0,   0,   0),  // 0  black
    QColor(170,   0,   0),  // 1  red
    QColor(  0, 170,   0),  // 2  green
    QColor(170, 170,   0),  // 3  yellow (dark)
    QColor(  0,   0, 170),  // 4  blue
    QColor(170,   0, 170),  // 5  magenta
    QColor(  0, 170, 170),  // 6  cyan
    QColor(170, 170, 170),  // 7  light grey
    QColor( 85,  85,  85),  // 8  dark grey  (bright black)
    QColor(255,  85,  85),  // 9  bright red
    QColor( 85, 255,  85),  // 10 bright green
    QColor(255, 255,  85),  // 11 bright yellow
    QColor( 85,  85, 255),  // 12 bright blue
    QColor(255,  85, 255),  // 13 bright magenta / pink
    QColor( 85, 255, 255),  // 14 bright cyan
    QColor(255, 255, 255),  // 15 white (bright white)
};

static const QColor DEFAULT_FG = QColor(170, 170, 170); // light grey
static const QColor DEFAULT_BG = QColor(  0,   0,   0); // black

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TerminalWidget::TerminalWidget(int backbufferCapacity, QWidget *parent)
    : QWidget(parent)
    , m_capacity(qMax(backbufferCapacity, 2))
    , m_head(0)
    , m_lineCount(1)       // always at least one open line
    , m_scrollOffset(0)
    , m_lineHeight(16)
    , m_charWidth(8)
    , m_ascent(12)
    , m_defaultFg(DEFAULT_FG)
    , m_defaultBg(DEFAULT_BG)
    , m_pendingCr(false)
{
    // Allocate buffer  -  one Line per slot, all empty
    m_buffer.resize(m_capacity);

    // Copy default palette
    for (int i = 0; i < PALETTE_SIZE; ++i)
    {
        m_palette[i] = DEFAULT_PALETTE[i];
    }

    // Scrollbar  -  positioned to the right, managed manually
    m_scrollbar = new QScrollBar(Qt::Vertical, this);
    m_scrollbar->setMinimum(0);
    m_scrollbar->setValue(0);
    connect(m_scrollbar, &QScrollBar::valueChanged, this,
            [this](int value)
            {
                // value = distance from top of backbuffer in lines
                // m_scrollOffset = distance from bottom
                const int total = m_lineCount;
                const int vis   = visibleLineCount();
                m_scrollOffset  = qMax(0, total - vis - value);
                update();
            });

    // Default font
    QFont font(QStringLiteral("Courier New"), 10);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    setTerminalFont(font);

    setFocusPolicy(Qt::NoFocus); // InputWidget owns focus
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(80 * m_charWidth, 24 * m_lineHeight);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void TerminalWidget::appendRun(const StyledRun &run)
{
    if (run.text.isEmpty())
    {
        return;
    }

    const bool wasAtBottom = isAtBottom();
    processRunText(run);

    updateScrollbar();

    if (wasAtBottom)
    {
        scrollToBottom();
    }

    update();
}

void TerminalWidget::clearScreen()
{
    for (Line &line : m_buffer)
    {
        line.clear();
    }
    m_head      = 0;
    m_lineCount = 1;
    m_scrollOffset = 0;
    m_pendingCr = false;
    updateScrollbar();
    update();
}

void TerminalWidget::setTerminalFont(const QFont &font)
{
    m_font = font;
    updateMetrics();
    updateScrollbar();
    update();
}

void TerminalWidget::setColour(int index, const QColor &colour)
{
    if (index >= 0 && index < PALETTE_SIZE)
    {
        m_palette[index] = colour;
        update();
    }
}

void TerminalWidget::setDefaultForeground(const QColor &colour)
{
    m_defaultFg = colour;
    update();
}

void TerminalWidget::setDefaultBackground(const QColor &colour)
{
    m_defaultBg = colour;
    update();
}

QColor TerminalWidget::colour(int index) const
{
    if (index >= 0 && index < PALETTE_SIZE)
    {
        return m_palette[index];
    }
    return m_defaultFg;
}

int TerminalWidget::lineCount() const
{
    // Excludes the open line
    return qMax(0, m_lineCount - 1);
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void TerminalWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setFont(m_font);

    const int widgetW   = width() - m_scrollbar->width();
    const int widgetH   = height();
    const int visLines  = visibleLineCount();
    const int total     = m_lineCount;

    // Fill background
    painter.fillRect(event->rect(), m_defaultBg);

    // Determine which logical line index to start rendering from.
    // Logical index 0 = oldest, (m_lineCount - 1) = open line.
    // We render at most visLines lines, ending at the open line minus
    // m_scrollOffset.
    const int lastVisible  = total - 1 - m_scrollOffset;
    const int firstVisible = qMax(0, lastVisible - visLines + 1);

    int y = 0;

    // If there are fewer lines than the visible area, push output to the
    // bottom (like a real terminal).
    const int linesAvailable = lastVisible - firstVisible + 1;
    if (linesAvailable < visLines)
    {
        y = (visLines - linesAvailable) * m_lineHeight;
    }

    for (int li = firstVisible; li <= lastVisible; ++li)
    {
        const Line &line = lineAt(li);
        int x = 0;

        for (const StyledRun &run : line)
        {
            if (run.text.isEmpty())
            {
                continue;
            }

            const QColor fg = resolveFg(run.fg);
            const QColor bg = resolveBg(run.bg);

            // Measure run width
            const int runW = run.text.length() * m_charWidth;

            // Background fill
            painter.fillRect(x, y, runW, m_lineHeight, bg);

            // Text
            QFont renderFont = m_font;
            renderFont.setBold(run.bold);
            painter.setFont(renderFont);
            painter.setPen(fg);
            painter.drawText(x, y + m_ascent, run.text);

            x += runW;
        }

        // Fill remaining line width with background
        if (x < widgetW)
        {
            painter.fillRect(x, y, widgetW - x, m_lineHeight, m_defaultBg);
        }

        y += m_lineHeight;
        if (y >= widgetH)
        {
            break;
        }
    }
}

void TerminalWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)

    // Position scrollbar on the right edge
    m_scrollbar->setGeometry(width() - m_scrollbar->sizeHint().width(), 0,
                             m_scrollbar->sizeHint().width(), height());
    updateScrollbar();
}

void TerminalWidget::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0)
    {
        return;
    }

    // Standard: 120 units = one notch = 3 lines
    const int lines = (delta / 120) * 3;
    const int newOffset = qBound(0,
                                 m_scrollOffset - lines,
                                 qMax(0, m_lineCount - visibleLineCount()));
    if (newOffset != m_scrollOffset)
    {
        m_scrollOffset = newOffset;
        // Sync scrollbar without triggering our valueChanged handler
        m_scrollbar->blockSignals(true);
        m_scrollbar->setValue(qMax(0, m_lineCount - visibleLineCount() - m_scrollOffset));
        m_scrollbar->blockSignals(false);
        update();
    }
}

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    // TerminalWidget does not accept keyboard input  -  InputWidget owns focus.
    // Page Up / Page Down are still handled here for scrollback convenience.
    if (event->key() == Qt::Key_PageUp)
    {
        const int newOffset = qMin(m_scrollOffset + visibleLineCount(),
                                   qMax(0, m_lineCount - visibleLineCount()));
        m_scrollOffset = newOffset;
        m_scrollbar->blockSignals(true);
        m_scrollbar->setValue(qMax(0, m_lineCount - visibleLineCount() - m_scrollOffset));
        m_scrollbar->blockSignals(false);
        update();
    }
    else if (event->key() == Qt::Key_PageDown)
    {
        m_scrollOffset = qMax(0, m_scrollOffset - visibleLineCount());
        m_scrollbar->blockSignals(true);
        m_scrollbar->setValue(qMax(0, m_lineCount - visibleLineCount() - m_scrollOffset));
        m_scrollbar->blockSignals(false);
        update();
    }
    else
    {
        QWidget::keyPressEvent(event);
    }
}

// ---------------------------------------------------------------------------
// Run text processing
// ---------------------------------------------------------------------------

void TerminalWidget::processRunText(const StyledRun &run)
{
    // Split on \n. Each segment between \n markers is appended to the open
    // line; a \n commits the open line and opens a new one.
    // \r is handled within each segment.

    const QString &text = run.text;
    int segStart = 0;

    for (int i = 0; i < text.length(); ++i)
    {
        const QChar ch = text[i];

        if (ch == QLatin1Char('\n'))
        {
            // Flush segment up to (not including) this \n
            const QString segment = text.mid(segStart, i - segStart);
            if (!segment.isEmpty())
            {
                StyledRun seg = run;
                seg.text = segment;
                appendSegmentToOpenLine(run, segment);
            }
            commitOpenLine();
            segStart = i + 1;
            m_pendingCr = false;
        }
        else if (ch == QLatin1Char('\r'))
        {
            // Flush segment up to \r then set pendingCr
            const QString segment = text.mid(segStart, i - segStart);
            if (!segment.isEmpty())
            {
                appendSegmentToOpenLine(run, segment);
            }
            carriageReturn();
            segStart = i + 1;
            m_pendingCr = true;
        }
    }

    // Remaining segment after last control character
    if (segStart < text.length())
    {
        appendSegmentToOpenLine(run, text.mid(segStart));
    }
}

void TerminalWidget::appendSegmentToOpenLine(const StyledRun &run,
                                             const QString &text)
{
    if (text.isEmpty())
    {
        return;
    }

    // If pendingCr was set, carriageReturn() already cleared the open line.
    m_pendingCr = false;

    Line &openLine = lineAt(m_lineCount - 1);

    // Coalesce with previous run if style matches
    if (!openLine.isEmpty())
    {
        StyledRun &last = openLine.last();
        if (last.fg == run.fg && last.bg == run.bg && last.bold == run.bold)
        {
            last.text += text;
            return;
        }
    }

    StyledRun seg = run;
    seg.text = text;
    openLine.append(seg);
}

void TerminalWidget::commitOpenLine()
{
    // The open line becomes a complete historical line.
    // Open a new empty line  -  advancing the head if the buffer is full.
    if (m_lineCount < m_capacity)
    {
        m_lineCount++;
    }
    else
    {
        // Buffer full  -  overwrite the oldest line
        m_head = (m_head + 1) % m_capacity;
        // m_lineCount stays at m_capacity
    }

    // Clear the new open line slot
    lineAt(m_lineCount - 1).clear();
}

void TerminalWidget::carriageReturn()
{
    // Discard the content of the open line  -  next text will overwrite from col 0.
    lineAt(m_lineCount - 1).clear();
    m_pendingCr = false;
}

// ---------------------------------------------------------------------------
// Circular buffer access
// ---------------------------------------------------------------------------

Line &TerminalWidget::lineAt(int logicalIndex)
{
    const int physIndex = (m_head + logicalIndex) % m_capacity;
    return m_buffer[physIndex];
}

const Line &TerminalWidget::lineAt(int logicalIndex) const
{
    const int physIndex = (m_head + logicalIndex) % m_capacity;
    return m_buffer[physIndex];
}

// ---------------------------------------------------------------------------
// Metrics and scrollbar
// ---------------------------------------------------------------------------

void TerminalWidget::updateMetrics()
{
    const QFontMetrics fm(m_font);
    m_lineHeight = fm.height();
    m_ascent     = fm.ascent();
    // For a monospaced font, all characters have the same advance width.
    m_charWidth  = fm.horizontalAdvance(QLatin1Char('W'));
    setMinimumSize(80 * m_charWidth, 24 * m_lineHeight);
}

void TerminalWidget::updateScrollbar()
{
    const int vis   = visibleLineCount();
    const int total = m_lineCount;
    const int maxScroll = qMax(0, total - vis);

    m_scrollbar->setMaximum(maxScroll);
    m_scrollbar->setPageStep(vis);
    m_scrollbar->setSingleStep(1);

    // Clamp scroll offset in case the buffer shrank
    m_scrollOffset = qBound(0, m_scrollOffset, maxScroll);
}

void TerminalWidget::scrollToBottom()
{
    m_scrollOffset = 0;
    m_scrollbar->blockSignals(true);
    m_scrollbar->setValue(m_scrollbar->maximum());
    m_scrollbar->blockSignals(false);
}

bool TerminalWidget::isAtBottom() const
{
    return m_scrollOffset == 0;
}

int TerminalWidget::visibleLineCount() const
{
    if (m_lineHeight <= 0)
    {
        return 1;
    }
    return qMax(1, height() / m_lineHeight);
}

// ---------------------------------------------------------------------------
// Colour resolution
// ---------------------------------------------------------------------------

QColor TerminalWidget::resolveFg(quint8 index) const
{
    if (index == COLOUR_DEFAULT)
    {
        return m_defaultFg;
    }
    if (index < PALETTE_SIZE)
    {
        return m_palette[index];
    }
    return m_defaultFg;
}

QColor TerminalWidget::resolveBg(quint8 index) const
{
    if (index == COLOUR_DEFAULT)
    {
        return m_defaultBg;
    }
    if (index < PALETTE_SIZE)
    {
        return m_palette[index];
    }
    return m_defaultBg;
}
