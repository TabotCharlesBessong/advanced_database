#pragma once

#include <cstdint>
#include <vector>

#include "Page.h"

namespace advdb {

// Implements a slotted-page layout on top of a Page's payload.
//
// Payload layout (all integers little-endian):
//   Bytes 0-3  : uint32_t  numSlots
//   Bytes 4-7  : uint32_t  freeOffset  (next byte available for record data)
//   Bytes 8..  : record data area (grows upward from 8)
//   ...free space...
//   payloadSize - numSlots*4 .. payloadSize : slot directory (grows downward)
//     Slot i entry at bytes [payloadSize-(i+1)*4 .. payloadSize-i*4]:
//       uint16_t offset   (byte 0-1): position of record within payload
//       uint16_t length   (byte 2-3): byte length; 0 means deleted
//
// A fresh page must be init()-ed before first use.
// After modifying, call commit() to write the buffer back into the Page.
class SlottedPage {
public:
    explicit SlottedPage(Page& page);

    // Initialise an empty slotted page (overwrites payload entirely).
    void init();

    // Populate internal buffer from the page's current payload.
    bool load();

    // Write internal buffer back into the page.
    void commit();

    // True if the page can hold a new record of 'dataSize' bytes.
    bool hasSpace(std::size_t dataSize) const;

    // Append a record; sets outSlotId and returns true on success.
    bool insertRecord(const std::vector<uint8_t>& data, uint16_t& outSlotId);

    // Copy a record's bytes into outData; returns false if deleted or
    // out-of-range.
    bool getRecord(uint16_t slotId, std::vector<uint8_t>& outData) const;

    uint32_t numSlots() const { return numSlots_; }

    bool isDeleted(uint16_t slotId) const;

private:
    uint32_t readU32(std::size_t offset) const;
    void writeU32(std::size_t offset, uint32_t v);
    uint16_t readU16(std::size_t offset) const;
    void writeU16(std::size_t offset, uint16_t v);

    // Byte position of slot i's entry in buf_.
    std::size_t slotOffset(uint32_t i) const;

    Page& page_;
    std::vector<uint8_t> buf_;
    uint32_t numSlots_{0};
    uint32_t freeOffset_{8};
};

} // namespace advdb
