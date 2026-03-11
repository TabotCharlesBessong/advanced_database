#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "StorageConfig.h"

namespace advdb {

#pragma pack(push, 1)
struct PageHeader {
    std::uint32_t pageId;
    std::uint16_t flags;
    std::uint16_t version;
    std::uint32_t checksum;
};
#pragma pack(pop)

class Page {
public:
    explicit Page(std::uint32_t pageSize = StorageConfig::kDefaultPageSize);

    void setPageId(std::uint32_t pageId);
    std::uint32_t pageId() const;

    void setFlags(std::uint16_t flags);
    std::uint16_t flags() const;

    std::size_t payloadSize() const;
    const std::vector<std::uint8_t>& payload() const;
    bool setPayload(const std::vector<std::uint8_t>& payload);

    std::vector<std::uint8_t> serialize() const;
    bool deserialize(const std::vector<std::uint8_t>& bytes);

private:
    static std::uint32_t computeChecksum(const std::vector<std::uint8_t>& payload);

    PageHeader header_;
    std::uint32_t pageSize_;
    std::vector<std::uint8_t> payload_;
};

} // namespace advdb
