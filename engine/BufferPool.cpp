#include "BufferPool.h"

#include <algorithm>

namespace advdb {

BufferPool::BufferPool(std::size_t poolSize, FileManager& fileManager, std::uint32_t pageSize)
    : poolSize_(poolSize),
      pageSize_(pageSize),
      fileManager_(fileManager),
      frames_() {
    frames_.reserve(poolSize_);
    for (std::size_t i = 0; i < poolSize_; ++i) {
        frames_.emplace_back(pageSize_);
        lru_.push_back(i);
    }
}

bool BufferPool::pinPage(std::uint32_t pageId, const Page** outPage) {
    Page* mutablePage = nullptr;
    if (!pinPageMutable(pageId, &mutablePage)) {
        return false;
    }
    *outPage = mutablePage;
    return true;
}

bool BufferPool::pinPageMutable(std::uint32_t pageId, Page** outPage) {
    auto it = pageTable_.find(pageId);
    if (it != pageTable_.end()) {
        Frame& frame = frames_[it->second];
        frame.pinCount += 1U;
        touchLRU(it->second);
        *outPage = &frame.page;
        return true;
    }

    const std::size_t victimIndex = findVictimFrame();
    if (victimIndex == poolSize_) {
        return false;
    }

    Frame& victim = frames_[victimIndex];
    const std::uint32_t previousPageId = victim.page.pageId();
    const bool hadPreviousPage = pageTable_.find(previousPageId) != pageTable_.end();

    if (hadPreviousPage) {
        if (!flushFrame(victimIndex)) {
            return false;
        }
        pageTable_.erase(previousPageId);
    }

    if (!loadPageIntoFrame(pageId, victimIndex)) {
        return false;
    }

    victim.pinCount = 1U;
    victim.dirty = false;
    pageTable_[pageId] = victimIndex;
    touchLRU(victimIndex);
    *outPage = &victim.page;
    return true;
}

bool BufferPool::unpinPage(std::uint32_t pageId, bool isDirty) {
    auto it = pageTable_.find(pageId);
    if (it == pageTable_.end()) {
        return false;
    }

    Frame& frame = frames_[it->second];
    if (frame.pinCount == 0U) {
        return false;
    }

    frame.pinCount -= 1U;
    frame.dirty = frame.dirty || isDirty;
    return true;
}

bool BufferPool::flushAll() {
    bool ok = true;
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        if (!flushFrame(i)) {
            ok = false;
        }
    }
    return ok && fileManager_.flush();
}

bool BufferPool::loadPageIntoFrame(std::uint32_t pageId, std::size_t frameIndex) {
    std::vector<std::uint8_t> raw(pageSize_, 0U);
    Frame& frame = frames_[frameIndex];

    const bool pageExists = fileManager_.readPage(pageId, raw, pageSize_);
    frame.page.setPageId(pageId);
    frame.page.setFlags(0U);

    if (!pageExists) {
        std::vector<std::uint8_t> emptyPayload(frame.page.payloadSize(), 0U);
        return frame.page.setPayload(emptyPayload);
    }

    return frame.page.deserialize(raw);
}

bool BufferPool::flushFrame(std::size_t frameIndex) {
    Frame& frame = frames_[frameIndex];
    const bool trackedPage = pageTable_.find(frame.page.pageId()) != pageTable_.end();
    if (!trackedPage || !frame.dirty) {
        return true;
    }

    const std::vector<std::uint8_t> raw = frame.page.serialize();
    if (!fileManager_.writePage(frame.page.pageId(), raw, pageSize_)) {
        return false;
    }

    frame.dirty = false;
    return true;
}

std::size_t BufferPool::findVictimFrame() const {
    for (std::size_t frameIndex : lru_) {
        const Frame& frame = frames_[frameIndex];
        if (frame.pinCount == 0U) {
            return frameIndex;
        }
    }
    return poolSize_;
}

void BufferPool::touchLRU(std::size_t frameIndex) {
    auto it = std::find(lru_.begin(), lru_.end(), frameIndex);
    if (it != lru_.end()) {
        lru_.erase(it);
    }
    lru_.push_back(frameIndex);
}

} // namespace advdb
