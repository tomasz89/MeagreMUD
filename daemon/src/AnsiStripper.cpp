#include "AnsiStripper.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AnsiStripper::AnsiStripper(QObject *parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void AnsiStripper::flush()
{
    if (m_buffer.isEmpty())
    {
        return;
    }

    emit strippedText(m_buffer);
    m_buffer.clear();
}

void AnsiStripper::reset()
{
    m_buffer.clear();
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void AnsiStripper::onRunReady(StyledRun run)
{
    // Style information is discarded — we only care about the text.
    const QString &text = run.text;

    for (int i = 0; i < text.length(); ++i)
    {
        const QChar ch = text.at(i);

        if (ch == QLatin1Char('\r'))
        {
            // Strip bare CR. CR+LF is handled when \n is processed next.
            continue;
        }

        if (ch == QLatin1Char('\n'))
        {
            emit strippedText(m_buffer);
            m_buffer.clear();
            continue;
        }

        m_buffer += ch;
    }
}
