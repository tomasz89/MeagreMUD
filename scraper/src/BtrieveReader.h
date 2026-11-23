#pragma once

/**
 * @file BtrieveReader.h
 * @brief Raw Btrieve 6.x fixed-format .dat file walker.
 *
 * No WBTRV32.DLL required. Reads the binary file directly.
 *
 * Format reference: MBBSEmu wiki, "Btrieve File Format"
 * https://wiki.mbbsemu.com/doku.php?id=mbbsemu:development:btrieve_file_format
 * (reverse-engineered from real MajorBBS/Worldgroup Btrieve files; far more
 * reliable than guessing from VB struct declarations alone).
 */

#include <QFile>
#include <QByteArray>
#include <QString>
#include <QVector>

/**
 * @brief Walks a Btrieve <= 6.0 fixed-format .dat file record by record.
 *
 * ## File layout
 * - Page 0 is always the File Control Record (FCR) -- file metadata.
 * - Page 1, if present, may be an ACS page (starts with bytes
 *   00 00 01 00 00 00 AC).
 * - Remaining pages are a mix of Key Pages and Data Pages. There is no
 *   guarantee data pages are contiguous or sequential -- each page must be
 *   individually classified.
 *
 * ## Page classification (relative to start of page)
 * - Key Page: 4-byte value 0xFFFFFFFF at offset 0x8.
 * - Data Page: bit 7 set in the byte at offset 0x5.
 * - Anything else (that isn't the FCR or ACS page) is skipped.
 *
 * ## FCR fields used (all offsets relative to file start, i.e. within page 0)
 * - 0x08 word   Page Length     (512-4096, multiple of 512)
 * - 0x14 word   Key Count
 * - 0x16 word   Record Length          (logical record size)
 * - 0x18 word   Physical Record Length (actual spacing between records;
 *                                       always >= Record Length)
 * - 0x1C dword  Record Count           (total live records in file)
 *
 * ## Data page record layout
 * Within a Data Page, records begin after a small page header. Each record
 * slot is Physical Record Length bytes. We treat any slot that is not
 * entirely zero bytes as plausibly live -- deleted records are typically
 * zeroed. Callers should still sanity-check field-level plausibility
 * (non-empty name, non-zero primary key) before inserting into the
 * database, which NativeDbImporter already does.
 *
 * Usage:
 * @code
 * BtrieveReader reader("/path/to/wccmp002.vir");
 * if (!reader.open()) { ... }
 * QByteArray rec;
 * while (!(rec = reader.nextRecord()).isNull()) {
 *     // process rec -- rec.size() == reader.recordLength()
 * }
 * @endcode
 */
class BtrieveReader
{
public:
    explicit BtrieveReader(const QString &path);
    ~BtrieveReader();

    bool open();
    void close();

    int recordLength() const { return m_recordLength; }
    int physicalRecordLength() const { return m_physicalRecordLength; }
    int pageSize() const { return m_pageSize; }
    int recordCount() const { return m_recordCount; }
    int dataPageCount() const { return m_dataPageOffsets.size(); }
    QString path() const { return m_path; }

    QByteArray nextRecord();
    void rewind();

    static quint8  rb1(const QByteArray &rec, int offset);
    static quint16 rb2(const QByteArray &rec, int offset);
    static quint32 rb4(const QByteArray &rec, int offset);
    static QString rbStr(const QByteArray &rec, int offset, int length);

private:
    bool readFcr();
    void classifyPages();
    static bool isAllZero(const QByteArray &data);

    QString  m_path;
    QFile    m_file;

    int      m_recordLength         = 0;
    int      m_physicalRecordLength = 0;
    int      m_pageSize             = 0;
    int      m_recordCount          = 0;

    QVector<qint64> m_dataPageOffsets;

    int    m_dataPageIndex = 0;
    qint64 m_slotInPage    = 0;

    static constexpr int FCR_PAGE_LENGTH_OFFSET     = 0x08;
    static constexpr int FCR_KEY_COUNT_OFFSET       = 0x14;
    static constexpr int FCR_RECORD_LENGTH_OFFSET   = 0x16;
    static constexpr int FCR_PHYS_RECORD_LEN_OFFSET = 0x18;
    static constexpr int FCR_RECORD_COUNT_OFFSET    = 0x1C;

    static constexpr int PAGE_TYPE_PROBE_OFFSET     = 0x05;
    static constexpr int KEY_PAGE_MARKER_OFFSET     = 0x08;
    static constexpr quint32 KEY_PAGE_MARKER_VALUE  = 0xFFFFFFFFu;

    static constexpr int DATA_PAGE_HEADER_SIZE      = 6;
};
