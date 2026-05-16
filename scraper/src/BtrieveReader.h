#pragma once

/**
 * @file BtrieveReader.h
 * @brief Raw Btrieve 6.x fixed-format .dat file walker.
 *
 * No WBTRV32.DLL required. Reads the binary file directly.
 */

#include <QFile>
#include <QByteArray>
#include <QString>

/**
 * @brief Walks a Btrieve 6.x fixed-format .dat file record by record.
 *
 * Btrieve 6.x on-disk layout:
 * - File header at byte 0 (512 bytes): contains record length, page size,
 *   record count.
 * - Data pages follow at page_size boundaries.
 * - Each data page has a 6-byte page header.
 * - Within each page, records are packed contiguously. Each record is
 *   preceded by a 1-byte usage flag: 0x00 = record in use, other = deleted.
 *
 * Usage:
 * @code
 * BtrieveReader reader("/path/to/w00mp002.dat");
 * if (!reader.open()) { ... }
 * qDebug() << "Record length:" << reader.recordLength();
 * QByteArray rec;
 * while (!(rec = reader.nextRecord()).isNull()) {
 *     // process rec
 * }
 * @endcode
 */
class BtrieveReader
{
public:
    explicit BtrieveReader(const QString &path);
    ~BtrieveReader();

    /**
     * @brief Open the file and read the header.
     * @return true if the file was opened and the header parsed successfully.
     */
    bool open();

    /// Close the file.
    void close();

    /// @return The fixed record length in bytes (from file header).
    int recordLength() const { return m_recordLength; }

    /// @return The page size in bytes (from file header).
    int pageSize() const { return m_pageSize; }

    /// @return Number of records reported in the file header.
    int recordCount() const { return m_recordCount; }

    /// @return The file path this reader was constructed with.
    QString path() const { return m_path; }

    /**
     * @brief Return the next live record.
     *
     * Skips deleted records (usage flag != 0x00).
     *
     * @return Raw record bytes (length == recordLength()), or a null
     *         QByteArray when there are no more records.
     */
    QByteArray nextRecord();

    /// Rewind to the first data page.
    void rewind();

    // --- Inline read helpers (little-endian) ---

    /// Read a 1-byte unsigned integer from a record at the given offset.
    static quint8 rb1(const QByteArray &rec, int offset);

    /// Read a 2-byte little-endian unsigned integer from a record.
    static quint16 rb2(const QByteArray &rec, int offset);

    /// Read a 4-byte little-endian unsigned integer from a record.
    static quint32 rb4(const QByteArray &rec, int offset);

    /**
     * @brief Read a fixed-length null-padded string from a record.
     *
     * Reads @p length bytes starting at @p offset, strips null bytes and
     * trailing whitespace.
     *
     * @param rec     Raw record bytes.
     * @param offset  Byte offset within the record.
     * @param length  Number of bytes to read.
     * @return        Trimmed QString (Latin-1 encoding assumed).
     */
    static QString rbStr(const QByteArray &rec, int offset, int length);

private:
    bool readHeader();
    bool advanceToNextPage();

    QString  m_path;
    QFile    m_file;

    int      m_recordLength = 0;
    int      m_pageSize     = 0;
    int      m_recordCount  = 0;

    // Page-walking state
    qint64   m_currentPageOffset  = 0;  ///< Byte offset of the current data page
    int      m_slotInPage         = 0;  ///< Current record slot within the page
    int      m_slotsPerPage       = 0;  ///< Number of record slots per page

    static constexpr int HEADER_SIZE    = 512;
    static constexpr int PAGE_HEADER_SIZE = 6;
};
