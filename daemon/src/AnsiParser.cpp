#include "AnsiParser.h"

#include <QDebug>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AnsiParser::AnsiParser(QObject *parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void AnsiParser::onData(const QByteArray &data)
{
    for (int i = 0; i < data.size(); ++i)
    {
        processChar(data[i]);
    }
}

void AnsiParser::reset()
{
    m_state     = State::Normal;
    m_pendingCR = false;
    m_runText.clear();
    m_paramBuf.clear();
    m_fg   = COLOUR_DEFAULT;
    m_bg   = COLOUR_DEFAULT;
    m_bold = false;
    m_runFg   = COLOUR_DEFAULT;
    m_runBg   = COLOUR_DEFAULT;
    m_runBold = false;
}

// ---------------------------------------------------------------------------
// Core state machine
// ---------------------------------------------------------------------------

void AnsiParser::processChar(char ch)
{
    switch (m_state)
    {
        // -------------------------------------------------------------------
        case State::Normal:
        // -------------------------------------------------------------------
        {
            if (ch == '\x1B')
            {
                // Start of escape sequence  -  flush what we have first
                flushRun();
                m_state = State::EscapeStart;
                return;
            }

            if (ch == '\r')
            {
                if (m_pendingCR)
                {
                    // Two consecutive \r  -  treat first as literal \r
                    m_runText += QLatin1Char('\r');
                }
                m_pendingCR = true;
                return;
            }

            if (ch == '\n')
            {
                if (m_pendingCR)
                {
                    // \r\n sequence  -  append as \r\n
                    m_runText += QStringLiteral("\r\n");
                    m_pendingCR = false;
                }
                else
                {
                    m_runText += QLatin1Char('\n');
                }
                return;
            }

            // Any other character clears a pending CR first
            if (m_pendingCR)
            {
                m_runText += QLatin1Char('\r');
                m_pendingCR = false;
            }

            m_runText += QLatin1Char(ch);
            return;
        }

        // -------------------------------------------------------------------
        case State::EscapeStart:
        // -------------------------------------------------------------------
        {
            if (ch == '[')
            {
                m_paramBuf.clear();
                m_state = State::CSI;
                return;
            }

            // Anything else: drop the ESC, re-process this character
            // as if we were in Normal state
            m_state = State::Normal;
            processChar(ch);
            return;
        }

        // -------------------------------------------------------------------
        case State::CSI:
        // -------------------------------------------------------------------
        {
            if (ch >= '0' && ch <= '9')
            {
                m_paramBuf += QLatin1Char(ch);
                m_state = State::CSIParams;
                return;
            }

            if (ch == ';')
            {
                // Empty first parameter (e.g. ESC[;32m)
                m_paramBuf += QLatin1Char(ch);
                m_state = State::CSIParams;
                return;
            }

            if (ch == 'm')
            {
                // ESC[m  -  implicit SGR reset (same as ESC[0m)
                applySGR({0});
                m_state = State::Normal;
                return;
            }

            if (ch == 'H')
            {
                // ESC[H  -  cursor home, drop silently
                m_state = State::Normal;
                return;
            }

            if (ch == '2')
            {
                // Could be ESC[2J  -  wait for J
                m_paramBuf += QLatin1Char(ch);
                m_state = State::CSIErase;
                return;
            }

            // Unknown CSI introducer  -  drop and return to Normal
            m_state = State::Normal;
            return;
        }

        // -------------------------------------------------------------------
        case State::CSIParams:
        // -------------------------------------------------------------------
        {
            if (ch >= '0' && ch <= '9')
            {
                m_paramBuf += QLatin1Char(ch);
                return;
            }

            if (ch == ';')
            {
                m_paramBuf += QLatin1Char(ch);
                return;
            }

            if (ch == 'm')
            {
                // SGR sequence complete  -  parse and apply
                applySGR(parseParams());
                m_paramBuf.clear();
                m_state = State::Normal;
                return;
            }

            // Unrecognised final byte  -  drop entire sequence
            m_paramBuf.clear();
            m_state = State::Normal;
            return;
        }

        // -------------------------------------------------------------------
        case State::CSIErase:
        // -------------------------------------------------------------------
        {
            if (ch == 'J')
            {
                // ESC[2J  -  screen clear
                flushRun();
                emit screenCleared();
                m_paramBuf.clear();
                m_state = State::Normal;
                return;
            }

            // Not a screen-clear sequence  -  drop and return to Normal
            m_paramBuf.clear();
            m_state = State::Normal;
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Run management
// ---------------------------------------------------------------------------

void AnsiParser::flushRun()
{
    if (m_runText.isEmpty())
    {
        return;
    }

    StyledRun run;
    run.fg   = m_runFg;
    run.bg   = m_runBg;
    run.bold = m_runBold;
    run.text = m_runText;

    m_runText.clear();

    emit runReady(run);
}

// ---------------------------------------------------------------------------
// SGR application
// ---------------------------------------------------------------------------

void AnsiParser::applySGR(const QVector<int> &params)
{
    // Flush any accumulated text with the OLD style before changing it
    flushRun();

    for (int param : params)
    {
        if (param == 0)
        {
            // Reset all
            m_fg   = COLOUR_DEFAULT;
            m_bg   = COLOUR_DEFAULT;
            m_bold = false;
        }
        else if (param == 1)
        {
            m_bold = true;
        }
        else if (param >= 30 && param <= 37)
        {
            // Normal fg colours: palette 0 - 7
            m_fg = static_cast<quint8>(param - 30);
        }
        else if (param == 39)
        {
            m_fg = COLOUR_DEFAULT;
        }
        else if (param >= 40 && param <= 47)
        {
            // Normal bg colours: palette 0 - 7
            m_bg = static_cast<quint8>(param - 40);
        }
        else if (param == 49)
        {
            m_bg = COLOUR_DEFAULT;
        }
        else if (param >= 90 && param <= 97)
        {
            // Bright fg colours: palette 8 - 15
            m_fg = static_cast<quint8>(param - 90 + 8);
        }
        else if (param >= 100 && param <= 107)
        {
            // Bright bg colours: palette 8 - 15
            m_bg = static_cast<quint8>(param - 100 + 8);
        }
        // All other codes silently ignored
    }

    // The new style applies to the next run
    m_runFg   = m_fg;
    m_runBg   = m_bg;
    m_runBold = m_bold;
}

// ---------------------------------------------------------------------------
// Parameter parsing
// ---------------------------------------------------------------------------

QVector<int> AnsiParser::parseParams() const
{
    QVector<int> result;

    if (m_paramBuf.isEmpty())
    {
        result.append(0);
        return result;
    }

    const QStringList parts = m_paramBuf.split(QLatin1Char(';'));
    result.reserve(parts.size());

    for (const QString &part : parts)
    {
        if (part.isEmpty())
        {
            result.append(0);
        }
        else
        {
            bool ok = false;
            const int value = part.toInt(&ok);
            if (ok)
            {
                result.append(value);
            }
            else
            {
                result.append(0);
            }
        }
    }

    return result;
}
