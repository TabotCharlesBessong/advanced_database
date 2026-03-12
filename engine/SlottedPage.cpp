#include "SlottedPage.h"

#include <cstddef>

namespace advdb {

SlottedPage::SlottedPage(Page& page) : page_(page) {}

void SlottedPage::init() {
    const std::size_t ps = page_.payloadSize();
    buf_.assign(ps, 0u);
    numSlots_ = 0u;
    freeOffset_ = 8u;
    writeU32(0u, numSlots_);
    writeU32(4u, freeOffset_);
    page_.setPayload(buf_);
}

bool SlottedPage::load() {
    buf_ = page_.payload();
    if (buf_.size() < 8u) {
        return false;
    }
    numSlots_ = readU32(0u);
    freeOffset_ = readU32(4u);
    // Basic sanity: freeOffset must be within payload and not overlap directory.
    if (freeOffset_ < 8u || freeOffset_ > buf_.size() ||
        freeOffset_ + numSlots_ * 4u > buf_.size()) {
        return false;
    }
    return true;
}

void SlottedPage::commit() {
    writeU32(0u, numSlots_);
    writeU32(4u, freeOffset_);
    page_.setPayload(buf_);
}

bool SlottedPage::hasSpace(std::size_t dataSize) const {
    // Need room for the data bytes AND one new 4-byte slot entry.
    const std::size_t slotDirSize = static_cast<std::size_t>(numSlots_) * 4u;
    if (freeOffset_ + slotDirSize + 4u > buf_.size()) {
        return false;
    }
    const std::size_t freeBytes = buf_.size() - freeOffset_ - slotDirSize - 4u;
    return dataSize <= freeBytes;
}

bool SlottedPage::insertRecord(const std::vector<uint8_t>& data, uint16_t& outSlotId) {
    if (!hasSpace(data.size())) {
        return false;
    }

    const uint16_t offset = static_cast<uint16_t>(freeOffset_);
    const uint16_t length = static_cast<uint16_t>(data.size());

    // Copy record data into buf at freeOffset.
    for (std::size_t i = 0; i < data.size(); ++i) {
        buf_[freeOffset_ + i] = data[i];
    }
    freeOffset_ += static_cast<uint32_t>(data.size());

    // Write slot entry (offset, length) at the next slot position.
    const std::size_t se = slotOffset(numSlots_);
    writeU16(se, offset);
    writeU16(se + 2u, length);

    outSlotId = static_cast<uint16_t>(numSlots_);
    ++numSlots_;
    return true;
}

bool SlottedPage::getRecord(uint16_t slotId, std::vector<uint8_t>& outData) const {
    if (slotId >= numSlots_) {
        return false;
    }
    const std::size_t se = slotOffset(slotId);
    const uint16_t offset = readU16(se);
    const uint16_t length = readU16(se + 2u);
    if (length == 0u) {
        // Deleted slot.
        return false;
    }
    if (static_cast<std::size_t>(offset) + length > buf_.size()) {
        return false;
    }
    outData.assign(buf_.begin() + offset, buf_.begin() + offset + length);
    return true;
}

bool SlottedPage::isDeleted(uint16_t slotId) const {
    if (slotId >= numSlots_) {
        return true;
    }
    const std::size_t se = slotOffset(slotId);
    return readU16(se + 2u) == 0u;
}

// --- Helpers ---

uint32_t SlottedPage::readU32(std::size_t offset) const {
    return static_cast<uint32_t>(buf_[offset]) |
           (static_cast<uint32_t>(buf_[offset + 1u]) << 8u) |
           (static_cast<uint32_t>(buf_[offset + 2u]) << 16u) |
           (static_cast<uint32_t>(buf_[offset + 3u]) << 24u);
}

void SlottedPage::writeU32(std::size_t offset, uint32_t v) {
    buf_[offset + 0u] = static_cast<uint8_t>(v & 0xFFu);
    buf_[offset + 1u] = static_cast<uint8_t>((v >> 8u) & 0xFFu);
    buf_[offset + 2u] = static_cast<uint8_t>((v >> 16u) & 0xFFu);
    buf_[offset + 3u] = static_cast<uint8_t>((v >> 24u) & 0xFFu);
}

uint16_t SlottedPage::readU16(std::size_t offset) const {
    return static_cast<uint16_t>(buf_[offset]) |
           static_cast<uint16_t>(static_cast<uint32_t>(buf_[offset + 1u]) << 8u);
}

void SlottedPage::writeU16(std::size_t offset, uint16_t v) {
    buf_[offset + 0u] = static_cast<uint8_t>(v & 0xFFu);
    buf_[offset + 1u] = static_cast<uint8_t>((v >> 8u) & 0xFFu);
}

std::size_t SlottedPage::slotOffset(uint32_t i) const {
    // Slot directory grows downward from the end.
    // Slot 0 is at the highest address: buf_.size() - 4
    // Slot i is at: buf_.size() - (i + 1) * 4
    return buf_.size() - (static_cast<std::size_t>(i) + 1u) * 4u;
}

} // namespace advdb
