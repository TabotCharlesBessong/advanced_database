#include "TableHeap.h"

#include <filesystem>

#include "RecordLayout.h"
#include "SlottedPage.h"
#include "StorageConfig.h"

namespace advdb {

TableHeap::TableHeap(std::string heapFilePath)
    : heapFilePath_(std::move(heapFilePath)),
      fileManager_(heapFilePath_),
      bufferPool_(8u, fileManager_) {}

TableHeap::~TableHeap() {
    close();
}

bool TableHeap::open() {
    return fileManager_.open(true);
}

void TableHeap::close() {
    bufferPool_.flushAll();
    fileManager_.close();
}

uint32_t TableHeap::pageCount() const {
    std::error_code ec;
    const auto size = std::filesystem::file_size(heapFilePath_, ec);
    if (ec || size == 0u) {
        return 0u;
    }
    return static_cast<uint32_t>(size / StorageConfig::kDefaultPageSize);
}

bool TableHeap::allocatePage(uint32_t& outPageId) {
    const uint32_t newId = pageCount();

    // Build a fresh empty page, serialize it, write to disk.
    Page freshPage(StorageConfig::kDefaultPageSize);
    freshPage.setPageId(newId);
    freshPage.setFlags(0u);

    SlottedPage sp(freshPage);
    sp.init();
    sp.commit();

    const std::vector<uint8_t> serialized = freshPage.serialize();
    if (!fileManager_.writePage(newId, serialized, StorageConfig::kDefaultPageSize)) {
        return false;
    }
    if (!fileManager_.flush()) {
        return false;
    }
    outPageId = newId;
    return true;
}

bool TableHeap::insertRow(const TableSchema& schema, const Row& row, RecordId& outRid) {
    const std::vector<uint8_t> encoded = RecordLayout::encode(schema, row);
    if (encoded.empty() && !row.empty()) {
        return false;
    }

    // Walk existing pages looking for one with enough free space.
    const uint32_t numPages = pageCount();
    for (uint32_t pid = 0u; pid < numPages; ++pid) {
        Page* pagePtr = nullptr;
        if (!bufferPool_.pinPageMutable(pid, &pagePtr)) {
            continue;
        }

        SlottedPage sp(*pagePtr);
        if (!sp.load()) {
            bufferPool_.unpinPage(pid, false);
            continue;
        }

        if (sp.hasSpace(encoded.size())) {
            uint16_t slotId = 0u;
            const bool ok = sp.insertRecord(encoded, slotId);
            sp.commit();
            bufferPool_.unpinPage(pid, true);
            if (ok) {
                outRid = RecordId{pid, slotId};
                return true;
            }
        } else {
            bufferPool_.unpinPage(pid, false);
        }
    }

    // No existing page had room — allocate a new one.
    uint32_t newPageId = 0u;
    if (!allocatePage(newPageId)) {
        return false;
    }

    Page* pagePtr = nullptr;
    if (!bufferPool_.pinPageMutable(newPageId, &pagePtr)) {
        return false;
    }

    SlottedPage sp(*pagePtr);
    if (!sp.load()) {
        bufferPool_.unpinPage(newPageId, false);
        return false;
    }

    uint16_t slotId = 0u;
    if (!sp.insertRecord(encoded, slotId)) {
        bufferPool_.unpinPage(newPageId, false);
        return false;
    }
    sp.commit();
    bufferPool_.unpinPage(newPageId, true);

    outRid = RecordId{newPageId, slotId};
    return true;
}

bool TableHeap::scanAll(const TableSchema& schema,
                        std::function<bool(const RecordId&, const Row&)> callback) {
    const uint32_t numPages = pageCount();
    for (uint32_t pid = 0u; pid < numPages; ++pid) {
        const Page* pagePtr = nullptr;
        if (!bufferPool_.pinPage(pid, &pagePtr)) {
            continue;
        }

        // SlottedPage needs a mutable Page reference for load/commit, but we
        // only read here.  Cast away const — we will not call commit().
        Page* mutablePage = const_cast<Page*>(pagePtr);
        SlottedPage sp(*mutablePage);
        if (!sp.load()) {
            bufferPool_.unpinPage(pid, false);
            continue;
        }

        const uint32_t slots = sp.numSlots();
        for (uint32_t s = 0u; s < slots; ++s) {
            const uint16_t slotId = static_cast<uint16_t>(s);
            if (sp.isDeleted(slotId)) {
                continue;
            }

            std::vector<uint8_t> encoded;
            if (!sp.getRecord(slotId, encoded)) {
                continue;
            }

            Row row;
            if (!RecordLayout::decode(schema, encoded, row)) {
                continue;
            }

            const RecordId rid{pid, slotId};
            if (!callback(rid, row)) {
                bufferPool_.unpinPage(pid, false);
                return true; // early stop requested by caller
            }
        }

        bufferPool_.unpinPage(pid, false);
    }
    return true;
}

} // namespace advdb
