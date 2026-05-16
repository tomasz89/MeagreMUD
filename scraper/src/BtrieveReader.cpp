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

    if (!readHeader())
    {
        m_file.close();
        return false;
    }

    // Slots per page: usable space divided by (1 flag byte + record length)
    const int usable = m_pageSize - PAGE_HEADER_SIZE;
    m_slotsPerPage   = usable / (1 + m_recordLength);

    // First data page starts immediately after the file header
    m_currentPageOffset = HEADER_SIZE;
    m_slotInPage        = 0;

    qDebug() << "BtrieveReader:" << m_path
             << "recordLength=" << m_recordLength
             << "pageSize="     << m_pageSize
             << "recordCount="  << m_recordCount
             << "slotsPerPage=" << m_slotsPerPage;

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
    while (true)
    {
        if (m_slotInPage >= m_slotsPerPage)
        {
            if (!advanceToNextPage())
            {
                return QByteArray();
            }
        }

        const qint64 slotOffset = m_currentPageOffset
                                 + PAGE_HEADER_SIZE
                                 + static_cast<qint64>(m_slotInPage)
                                   * (1 + m_recordLength);

        if (!m_file.seek(slotOffset))
        {
            return QByteArray();
        }

        char flag = 0;
        if (m_file.read(&flag, 1) != 1)
        {
            return QByteArray();
        }

        m_slotInPage++;

        if (static_cast<unsigned char>(flag) != 0x00)
        {
            continue; // deleted or unused
        }

        const QByteArray record = m_file.read(m_recordLength);
        if (record.size() != m_recordLength)
        {
            return QByteArray();
        }

        return record;
    }
}

void BtrieveReader::rewind()
{
    m_currentPageOffset = HEADER_SIZE;
    m_slotInPage        = 0;
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
// Private helpers
// ---------------------------------------------------------------------------

bool BtrieveReader::readHeader()
{
    if (m_file.size() < HEADER_SIZE)
    {
        qWarning() << "BtrieveReader: file too small:" << m_path;
        return false;
    }

    m_file.seek(0);
    const QByteArray hdr = m_file.read(HEADER_SIZE);
    if (hdr.size() < HEADER_SIZE)
    {
        qWarning() << "BtrieveReader: short header read:" << m_path;
        return false;
    }

    // Btrieve 6.x header (all little-endian):
    // 0x00 uint16 record_length
    // 0x02 uint16 page_size
    // 0x04 uint16 number_of_keys
    // 0x06 uint16 record_count_low
    // 0x08 uint16 flags
    // 0x0A uint16 record_count_high
    m_recordLength = static_cast<int>(
        static_cast<quint16>(static_cast<quint8>(hdr[0]))
      | (static_cast<quint16>(static_cast<quint8>(hdr[1])) << 8));

    m_pageSize = static_cast<int>(
        static_cast<quint16>(static_cast<quint8>(hdr[2]))
      | (static_cast<quint16>(static_cast<quint8>(hdr[3])) << 8));

    const quint16 cntLow  = static_cast<quint16>(static_cast<quint8>(hdr[6]))
                          | (static_cast<quint16>(static_cast<quint8>(hdr[7])) << 8);
    const quint16 cntHigh = static_cast<quint16>(static_cast<quint8>(hdr[10]))
                          | (static_cast<quint16>(static_cast<quint8>(hdr[11])) << 8);

    m_recordCount = static_cast<int>(cntLow)
                  | (static_cast<int>(cntHigh) << 16);

    if (m_recordLength <= 0 || m_pageSize <= PAGE_HEADER_SIZE)
    {
        qWarning() << "BtrieveReader: invalid header in" << m_path
                   << "recordLength=" << m_recordLength
                   << "pageSize="     << m_pageSize;
        return false;
    }

    return true;
}

bool BtrieveReader::advanceToNextPage()
{
    m_currentPageOffset += m_pageSize;
    m_slotInPage = 0;
    return m_currentPageOffset < m_file.size();
}
