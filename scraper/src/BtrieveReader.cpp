#include "BtrieveReader.h"
#include <algorithm>

BtrieveReader::BtrieveReader(const QString &path, ByteOrder order) 
    : m_path(path), m_order(order) 
{
}

BtrieveReader::~BtrieveReader() 
{
    close();
}

void BtrieveReader::close() 
{
    if (m_file.isOpen()) 
    {
        m_file.close();
    }
}

void BtrieveReader::rewind() 
{
    m_currentPageOffset = HEADER_SIZE;
    m_slotInPage = 0;
}

bool BtrieveReader::open() 
{
    m_file.setFileName(m_path);
    if (!m_file.open(QIODevice::ReadOnly)) 
    {
        return false;
    }
    
    if (m_file.size() > MAX_FILE_SIZE) 
    {
        qWarning() << "BtrieveReader: File exceeds safety limit:" << m_path;
        return false;
    }

    if (!readHeader()) 
    {
        // If the first header fails, try to see if there's a valid segment later
        return moveToNextSegment();
    }
    
    return true;
}

bool BtrieveReader::readHeader()
{
    if (m_file.size() < HEADER_SIZE) return false;

    m_file.seek(0);
    const QByteArray hdr = m_file.read(1024);
    if (hdr.size() < 1024) return false;

    // The known lengths we are looking for
    const QList<int> targetLengths = {1544, 756, 1072};

    // We scan every byte as a potential start of the length field
    for (int i = 0; i < 512; ++i)
    {
        for (ByteOrder order : {ByteOrder::LittleEndian, ByteOrder::BigEndian})
        {
            int testLen = static_cast<int>(assemble16(hdr, i, order));

            // If we find one of our known record lengths...
            if (std::find(targetLengths.begin(), targetLengths.end(), testLen) != targetLengths.end())
            {
                // ...then the Page Size is almost always 2 or 4 bytes later
                for (int pOffset : {i + 2, i + 4})
                {
                    int testPage = static_cast<int>(assemble16(hdr, pOffset, order));

                    // Valid page size check: must be > length and a standard power of 2 or large block
                    if (testPage > testLen && testPage <= 65535 && (testPage % 2 == 0))
                    {
                        // We found a match! Now get the count.
                        // The count in your files follows the page size.
                        const quint16 cntLow  = assemble16(hdr, pOffset + 4, order);
                        const quint16 cntHigh = assemble16(hdr, pOffset + 8, order);

                        m_recordLength = testLen;
                        m_pageSize = testPage;
                        m_recordCount = (static_cast<quint64>(cntHigh) << 16) | cntLow;

                        // Data starts after the header (512) + the offset we found
                        m_currentPageOffset = HEADER_SIZE + i;
                        m_slotsPerPage = (m_pageSize - PAGE_HEADER_SIZE) / (1 + m_recordLength);
                        m_slotInPage = 0;

                        qDebug() << "[BtrieveReader] MATCH FOUND!"
                                 << "Offset:" << i
                                 << "Len:" << m_recordLength
                                 << "Page:" << m_pageSize
                                 << "Count:" << m_recordCount;
                        return true;
                    }
                }
            }
        }
    }

    qWarning() << "BtrieveReader: Failed to find valid record/page geometry in" << m_path;
    return false;
}


bool BtrieveReader::moveToNextSegment()
{
    qint64 currentPos = m_file.pos();
    qint64 fileSize = m_file.size();
    
    // Search for "FC" signature in the file
    m_file.seek(currentPos);
    QByteArray remaining = m_file.readAll();
    
    int index = remaining.indexOf("\x46\x43"); // "FC"
    
    if (index != -1) 
    {
        qint64 nextSegmentStart = currentPos + index;
        m_file.seek(nextSegmentStart);
        
        if (readHeader()) 
        {
            return true;
        }
    }

    return false;
}

bool BtrieveReader::advanceToNextPage()
{
    m_currentPageOffset += m_pageSize;
    m_slotInPage = 0;
    return (m_currentPageOffset < m_file.size());
}

QByteArray BtrieveReader::nextRecord() 
{
    while (true) 
    {
        if (m_slotInPage >= m_slotsPerPage) 
        {
            if (!advanceToNextPage()) 
            {
                if (moveToNextSegment()) 
                {
                    qDebug() << "[BtrieveReader] Transitioned to new segment.";
                    continue;
                }
                return QByteArray();
            }
        }

        const qint64 slotOffset = m_currentPageOffset + PAGE_HEADER_SIZE + (static_cast<qint64>(m_slotInPage) * (1 + m_recordLength));
        
        if (slotOffset >= m_file.size()) 
        {
            if (moveToNextSegment()) 
            {
                continue;
            }
            return QByteArray();
        }

        const int readLen = 1 + m_recordLength;
        if (slotOffset + readLen > m_file.size()) 
        {
            if (moveToNextSegment()) 
            {
                continue;
            }
            return QByteArray();
        }

        m_file.seek(slotOffset);
        QByteArray buffer = m_file.read(readLen);
        
        if (buffer.size() < readLen) 
        {
            if (moveToNextSegment()) 
            {
                continue;
            }
            return QByteArray();
        }

        if (buffer.at(0) == 0x00) 
        {
            m_slotInPage++;
            return buffer.mid(1); 
        } 
        else 
        {
            m_slotInPage++;
            continue;
        }
    }
}

// --- Portable Assembly Helpers ---

quint8 BtrieveReader::assemble8(const QByteArray &rec, int offset, ByteOrder order) 
{
    if (offset < 0 || offset >= rec.size()) 
    {
        return 0;
    }
    return static_cast<quint8>(rec.at(offset));
}

quint16 BtrieveReader::assemble16(const QByteArray &rec, int offset, ByteOrder order) 
{
    if (offset < 0 || offset + 1 >= rec.size()) 
    {
        return 0;
    }
    quint8 b0 = static_cast<quint8>(rec.at(offset));
    quint8 b1 = static_cast<quint8>(rec.at(offset + 1));
    
    if (order == ByteOrder::LittleEndian) 
    {
        return (static_cast<quint16>(b0)) | (static_cast<quint16>(b1) << 8);
    } 
    else 
    {
        return (static_cast<quint16>(b0) << 8) | (static_cast<quint16>(b1));
    }
}

quint32 BtrieveReader::assemble32(const QByteArray &rec, int offset, ByteOrder order) 
{
    if (offset < 0 || offset + 3 >= rec.size()) 
    {
        return 0;
    }
    quint8 b0 = static_cast<quint8>(rec.at(offset));
    quint8 b1 = static_cast<quint8>(rec.at(offset + 1));
    quint8 b2 = static_cast<quint8>(rec.at(offset + 2));
    quint8 b3 = static_cast<quint8>(rec.at(offset + 3));

    if (order == ByteOrder::LittleEndian) 
    {
        return (static_cast<quint32>(b0)) | (static_cast<quint32>(b1) << 8) |
               (static_cast<quint32>(b2) << 16) | (static_cast<quint32>(b3) << 24);
    } 
    else 
    {
        return (static_cast<quint32>(b0) << 24) | (static_cast<quint32>(b1) << 16) |
               (static_cast<quint32>(b2) << 8) | (static_cast<quint32>(b3));
    }
}

QString BtrieveReader::assembleStr(const QByteArray &rec, int offset, int length) 
{
    if (offset < 0 || offset >= rec.size()) 
    {
        return QString();
    }
    
    int available = static_cast<int>(rec.size()) - offset;
    int actualLen = std::min(length, available);
    
    if (actualLen <= 0) 
    {
        return QString();
    }

    QByteArray slice = rec.mid(offset, actualLen);
    int nullPos = slice.indexOf('\0');
    if (nullPos != -1) 
    {
        slice.resize(nullPos);
    }
    
    return QString::fromLatin1(slice).trimmed();
}
