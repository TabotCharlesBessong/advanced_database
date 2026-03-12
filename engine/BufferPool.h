#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

#include "FileManager.h"
#include "Page.h"
#include "StorageConfig.h"

namespace advdb {

class BufferPool {
public:
    BufferPool(std::size_t poolSize, FileManager& fileManager, std::uint32_t pageSize = StorageConfig::kDefaultPageSize);

    bool pinPage(std::uint32_t pageId, const Page** outPage);
    bool pinPageMutable(std::uint32_t pageId, Page** outPage);
    bool unpinPage(std::uint32_t pageId, bool isDirty);
    bool flushAll();

private:
    struct Frame {
        Page page;
        std::uint32_t pinCount;
        bool dirty;

        explicit Frame(std::uint32_t pageSize) : page(pageSize), pinCount(0U), dirty(false) {}
    };

    bool loadPageIntoFrame(std::uint32_t pageId, std::size_t frameIndex);
    bool flushFrame(std::size_t frameIndex);
    std::size_t findVictimFrame() const;
    void touchLRU(std::size_t frameIndex);

    std::size_t poolSize_;
    std::uint32_t pageSize_;
    FileManager& fileManager_;
    std::vector<Frame> frames_;
    std::unordered_map<std::uint32_t, std::size_t> pageTable_;
    std::list<std::size_t> lru_;
};

} // namespace advdb
