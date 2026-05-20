/**
 * @file BtrieveReader.h
 * @brief Hardened, portable, multi-segment Btrieve 6.x file walker.
 *
 * Supports files containing concatenated Btrieve segments starting with "FC" signature.
 */  
#pragma once  

#include <QFile>
#include <QByteArray>
#include <QString>  
#include <QDebug>

/**
* @brief Walks a Btrieve 6.x fixed-format .dat file record by record.
*
* Supports concatenated segments. Each segment starts with an 'FC' signature.
*
* Usage:
* @code
* BtrieveReader reader("/path/to/w00mp002.dat", BtrieveReader::ByteOrder::BigEndian);
* if (!reader.open()) { ... }
* QByteArray rec;
* while (!(rec = reader.nextRecord()).isEmpty()) {
*     // process rec
* }
* @endcode
*/
class BtrieveReader
{
public:
    /**
     * @brief Endianness configuration for portable parsing.
     */
    enum class ByteOrder {
        LittleEndian,
        BigEndian
    };

    explicit BtrieveReader(const QString &path, ByteOrder order = ByteOrder::LittleEndian);
    ~BtrieveReader();  

    /**
     * @brief Open the file and read the first header.
     * @return true if the file was opened and the header parsed successfully.
     */
    bool open();

    /// Close the file.
    void close();

    /// @return Rewind to the first data page of the current segment.
    void rewind();

    /// @return The fixed record length in bytes (from file header).
    int recordLength() const { return m_recordLength; }

    /// @return The page size in bytes (from file header).
    int pageSize() const { return m_pageSize; }

    /// @return Total number of records across all segments found so far.
    quint64 recordCount() const { return m_recordCount; }

    /// @return The file path this reader was constructed with.
    QString path() const { return m_path; }

    /**
     * @brief Return the next live record.
     *
     * Automatically jumps to the next Btrieve segment if the current one ends.
     * Skips deleted records (usage flag != 0x00).
     *
     * @return Raw record bytes (length == recordLength()), or a null
     *         QByteArray when no more segments exist.
     */
    QByteArray nextRecord();

    /**
     * @brief Moves the reader to the next 'FC' signature segment in the file.
     * @return true if a new segment was found and initialized.
     */
    bool moveToNextSegment();

    // --- Portable Assembly Helpers (Public for Importer access) ---

    /**
     * @brief Read a 1-byte unsigned integer from a record at the given offset.
     */
    static quint8  assemble8(const QByteArray &rec, int offset, ByteOrder order = ByteOrder::LittleEndian);

    /**
     * @brief Read a 2-byte integer from a record at the given offset.
     */
    static quint16 assemble16(const QByteArray &rec, int offset, ByteOrder order);

    /**
     * @brief Read a 4-byte integer from a record at the given offset.
     */
    static quint32 assemble32(const QByteArray &rec, int offset, ByteOrder order);

    /**
     * @brief Read a fixed-length null-padded string from a record.
     *
     * @param rec     Raw record bytes.
     * @param offset  Byte offset within the record.
     * @param length  Number of bytes to read.
     * @return        Trimmed QString (Latin-1 encoding assumed).
     */
    static QString assembleStr(const QByteArray &rec, int offset, int length);

private:
    bool readHeader();
    bool advanceToNextPage();

    QString  m_path;
    QFile    m_file;
    ByteOrder m_order;

    int      m_recordLength = 0;
    int      m_pageSize = 0;
    quint64  m_recordCount = 0;  

    // Page-walking state
    qint64   m_currentPageOffset = 0;  ///< Byte offset of the current data page
    int      m_slotInPage = 0;         ///< Current record slot within the page  
    int      m_slotsPerPage = 0;       ///< Number of record slots per page  

    static constexpr int HEADER_SIZE = 512;
    static constexpr int PAGE_HEADER_SIZE = 6;
    static constexpr qint64 MAX_FILE_SIZE = 4ULL * 1024 * 1024 * 1024; // 4GB safety cap
};
