#include "BtrieveReader.h"

#include <QDebug>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

BtrieveReader::BtrieveReader(const QString &path)
    : m_path(path)
{
}

BtrieveReader::~BtrieveReader()
{
    close();
}

// ---------------------------------------------------------------------------
// Open / close
// ---------------------------------------------------------------------------

bool BtrieveReader::open()
{
    m_file.setFileName(m_path);
    if (!m_file.open(QIODevice::ReadOnly))
    {
        qWarning() << "BtrieveReader: cannot open" << m_path
                   << m_file.errorString();
        return false;
    }

    if (!readFcr())
    {
        m_file.close();
        return false;
    }

    classifyPages();

    m_dataPageIndex = 0;
    m_slotInPage    = DATA_PAGE_HEADER_SIZE;

    qDebug() << "BtrieveReader:" << m_path
             << "pageSize="             << m_pageSize
             << "recordLength="         << m_recordLength
             << "physicalRecordLength=" << m_physicalRecordLength
             << "recordCount="          << m_recordCount
             << "dataPages="            << m_dataPageOffsets.size();

    return true;
}

void BtrieveReader::close()
{
    if (m_file.isOpen())
    {
        m_file.close();
    }
}

// ---------------------------------------------------------------------------
// Record iteration
// ---------------------------------------------------------------------------

QByteArray BtrieveReader::nextRecord()
{
    while (m_dataPageIndex < m_dataPageOffsets.size())
    {
        const qint64 pageOffset = m_dataPageOffsets.at(m_dataPageIndex);

        if (m_slotInPage + m_physicalRecordLength > m_pageSize)
        {
            // No room for another full slot in this page -- advance
            m_dataPageIndex++;
            m_slotInPage = DATA_PAGE_HEADER_SIZE;
            continue;
        }

        const qint64 absoluteOffset = pageOffset + m_slotInPage;
        m_slotInPage += m_physicalRecordLength;

        if (!m_file.seek(absoluteOffset))
        {
            m_dataPageIndex++;
            m_slotInPage = DATA_PAGE_HEADER_SIZE;
            continue;
        }

        const QByteArray slot = m_file.read(m_physicalRecordLength);
        if (slot.size() < m_recordLength)
        {
            // Short read -- likely end of file mid-page
            m_dataPageIndex++;
            m_slotInPage = DATA_PAGE_HEADER_SIZE;
            continue;
        }

        const QByteArray record = slot.left(m_recordLength);

        if (isAllZero(record))
        {
            // Treated as deleted/unused -- keep scanning
            continue;
        }

        return record;
    }

    return QByteArray();
}

void BtrieveReader::rewind()
{
    m_dataPageIndex = 0;
    m_slotInPage    = DATA_PAGE_HEADER_SIZE;
}

// ---------------------------------------------------------------------------
// Static read helpers (little-endian)
// ---------------------------------------------------------------------------

quint8 BtrieveReader::rb1(const QByteArray &rec, int offset)
{
    if (offset < 0 || offset >= rec.size())
    {
        return 0;
    }
    return static_cast<quint8>(rec.at(offset));
}

quint16 BtrieveReader::rb2(const QByteArray &rec, int offset)
{
    if (offset < 0 || offset + 1 >= rec.size())
    {
        return 0;
    }
    return static_cast<quint16>(rb1(rec, offset))
         | (static_cast<quint16>(rb1(rec, offset + 1)) << 8);
}

quint32 BtrieveReader::rb4(const QByteArray &rec, int offset)
{
    if (offset < 0 || offset + 3 >= rec.size())
    {
        return 0;
    }
    return static_cast<quint32>(rb2(rec, offset))
         | (static_cast<quint32>(rb2(rec, offset + 2)) << 16);
}

QString BtrieveReader::rbStr(const QByteArray &rec, int offset, int length)
{
    if (offset < 0 || offset >= rec.size())
    {
        return QString();
    }
    const int actual = qMin(length, rec.size() - offset);
    QString result = QString::fromLatin1(rec.constData() + offset, actual);
    result.remove(QLatin1Char('\0'));
    return result.trimmed();
}

// ---------------------------------------------------------------------------
// Private -- FCR parsing
// ---------------------------------------------------------------------------

bool BtrieveReader::readFcr()
{
    // The FCR occupies the first page. We don't know the page size yet,
    // so read a generous chunk (4096 is the max Btrieve page size) and
    // re-seek later once page size is known.
    if (!m_file.seek(0))
    {
        return false;
    }

    const QByteArray fcr = m_file.read(4096);
    if (fcr.size() < 0x20)
    {
        qWarning() << "BtrieveReader: file too small for a valid FCR:"
                   << m_path;
        return false;
    }

    m_pageSize = static_cast<int>(rb2(fcr, FCR_PAGE_LENGTH_OFFSET));
    const quint16 keyCount = rb2(fcr, FCR_KEY_COUNT_OFFSET);
    Q_UNUSED(keyCount)

    m_recordLength         = static_cast<int>(rb2(fcr, FCR_RECORD_LENGTH_OFFSET));
    m_physicalRecordLength = static_cast<int>(rb2(fcr, FCR_PHYS_RECORD_LEN_OFFSET));
    m_recordCount          = static_cast<int>(rb4(fcr, FCR_RECORD_COUNT_OFFSET));

    if (m_pageSize < 512 || m_pageSize > 4096 || (m_pageSize % 512) != 0)
    {
        qWarning() << "BtrieveReader: implausible page size" << m_pageSize
                   << "in" << m_path << "-- aborting";
        return false;
    }

    if (m_recordLength <= 0)
    {
        qWarning() << "BtrieveReader: implausible record length"
                   << m_recordLength << "in" << m_path;
        return false;
    }

    if (m_physicalRecordLength < m_recordLength)
    {
        // Some files report 0 here; fall back to logical record length.
        m_physicalRecordLength = m_recordLength;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Private -- page classification
// ---------------------------------------------------------------------------

void BtrieveReader::classifyPages()
{
    m_dataPageOffsets.clear();

    const qint64 fileSize = m_file.size();
    qint64 offset = m_pageSize; // skip page 0 (FCR)

    while (offset + m_pageSize <= fileSize)
    {
        if (!m_file.seek(offset))
        {
            break;
        }

        const QByteArray pageHeader = m_file.read(16);
        if (pageHeader.size() < 16)
        {
            break;
        }

        const quint32 keyMarker = rb4(pageHeader, KEY_PAGE_MARKER_OFFSET);
        const quint8  typeProbe = rb1(pageHeader, PAGE_TYPE_PROBE_OFFSET);

        const bool isKeyPage  = (keyMarker == KEY_PAGE_MARKER_VALUE);
        const bool isDataPage = !isKeyPage && ((typeProbe & 0x80) != 0);

        if (isDataPage)
        {
            m_dataPageOffsets.append(offset);
        }

        offset += m_pageSize;
    }

    qDebug() << "BtrieveReader: classified" << m_dataPageOffsets.size()
             << "data pages out of"
             << ((fileSize / m_pageSize) - 1) << "candidate pages in"
             << m_path;
}

// ---------------------------------------------------------------------------
// Private -- helpers
// ---------------------------------------------------------------------------

bool BtrieveReader::isAllZero(const QByteArray &data)
{
    for (char c : data)
    {
        if (c != 0)
        {
            return false;
        }
    }
    return true;
}
