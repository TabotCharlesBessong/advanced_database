#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "BufferPool.h"
#include "CatalogTypes.h"
#include "FileManager.h"
#include "Value.h"

namespace advdb {

// Identifies a row by its page and slot within that page.
struct RecordId {
    uint32_t pageId{0};
    uint16_t slotId{0};
};

// Manages row-level storage for a single table using a heap file.
//
// Each table has one heap file (<dbPath>.<tableName>.heap).
// The file is a sequence of fixed-size pages; each page uses a SlottedPage
// layout.  BufferPool provides caching; FileManager handles raw I/O.
class TableHeap {
public:
    explicit TableHeap(std::string heapFilePath);
    ~TableHeap();

    bool open();
    void close();

    // Serialise 'row' according to 'schema' and store it; sets outRid.
    bool insertRow(const TableSchema& schema, const Row& row, RecordId& outRid);

    // Sequential scan: invokes callback(rid, row) for every non-deleted row.
    // If callback returns false the scan stops early.
    bool scanAll(const TableSchema& schema,
                 std::function<bool(const RecordId&, const Row&)> callback);

private:
    // Returns the number of whole pages currently on disk.
    uint32_t pageCount() const;

    // Writes a fresh empty SlottedPage at the next available pageId.
    bool allocatePage(uint32_t& outPageId);

    std::string heapFilePath_;
    FileManager fileManager_;
    BufferPool bufferPool_;
};

} // namespace advdb
