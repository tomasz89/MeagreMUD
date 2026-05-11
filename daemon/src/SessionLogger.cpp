#include "SessionLogger.h"

#include <QDir>
#include <QDebug>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SessionLogger::SessionLogger(QObject *parent)
    : QObject(parent)
{
}

SessionLogger::~SessionLogger()
{
    stop();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SessionLogger::configure(const QString &logPath, bool timestampLines)
{
    if (m_file.isOpen())
    {
        m_stream.flush();
        m_file.close();
    }

    m_logPath        = logPath;
    m_timestampLines = timestampLines;
}

void SessionLogger::stop()
{
    if (!m_file.isOpen())
    {
        return;
    }

    m_stream.flush();
    m_file.close();

    qDebug() << "SessionLogger: closed log" << m_logPath;
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void SessionLogger::onStrippedText(const QString &text)
{
    if (m_logPath.isEmpty())
    {
        return;
    }

    if (!m_file.isOpen())
    {
        if (!openFile())
        {
            return;
        }
    }

    if (m_timestampLines)
    {
        m_stream << QDateTime::currentDateTime().toString(
                        QStringLiteral("HH:mm:ss"))
                 << QLatin1Char(' ')
                 << text
                 << QLatin1Char('\n');
    }
    else
    {
        m_stream << text << QLatin1Char('\n');
    }

    // Flush after every line so the log is complete if the daemon crashes
    m_stream.flush();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool SessionLogger::openFile()
{
    // Ensure the directory exists
    const QFileInfo info(m_logPath);
    const QDir dir = info.absoluteDir();
    if (!dir.exists())
    {
        if (!dir.mkpath(dir.absolutePath()))
        {
            qWarning() << "SessionLogger: could not create log directory:"
                       << dir.absolutePath();
            return false;
        }
    }

    m_file.setFileName(m_logPath);
    if (!m_file.open(QIODevice::Append | QIODevice::Text))
    {
        qWarning() << "SessionLogger: could not open log file:"
                   << m_logPath
                   << m_file.errorString();
        return false;
    }

    m_stream.setDevice(&m_file);
    m_stream.setEncoding(QStringConverter::Utf8);

    qDebug() << "SessionLogger: opened log" << m_logPath;
    return true;
}
