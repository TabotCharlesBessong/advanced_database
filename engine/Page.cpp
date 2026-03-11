#include "Page.h"

#include <algorithm>
#include <cstring>

namespace advdb {

Page::Page(std::uint32_t pageSize)
    : header_{0U, 0U, StorageConfig::kPageFormatVersion, 0U},
      pageSize_(pageSize),
      payload_(pageSize - static_cast<std::uint32_t>(sizeof(PageHeader)), 0U) {}

void Page::setPageId(std::uint32_t pageId) {
    header_.pageId = pageId;
}

std::uint32_t Page::pageId() const {
    return header_.pageId;
}

void Page::setFlags(std::uint16_t flags) {
    header_.flags = flags;
}

std::uint16_t Page::flags() const {
    return header_.flags;
}

std::size_t Page::payloadSize() const {
    return payload_.size();
}

const std::vector<std::uint8_t>& Page::payload() const {
    return payload_;
}

bool Page::setPayload(const std::vector<std::uint8_t>& payload) {
    if (payload.size() > payload_.size()) {
        return false;
    }

    std::fill(payload_.begin(), payload_.end(), 0U);
    std::copy(payload.begin(), payload.end(), payload_.begin());
    return true;
}

std::vector<std::uint8_t> Page::serialize() const {
    std::vector<std::uint8_t> bytes(pageSize_, 0U);

    PageHeader header = header_;
    header.version = StorageConfig::kPageFormatVersion;
    header.checksum = computeChecksum(payload_);

    std::memcpy(bytes.data(), &header, sizeof(PageHeader));
    std::memcpy(bytes.data() + sizeof(PageHeader), payload_.data(), payload_.size());
    return bytes;
}

bool Page::deserialize(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() != pageSize_) {
        return false;
    }

    PageHeader incomingHeader{};
    std::memcpy(&incomingHeader, bytes.data(), sizeof(PageHeader));

    if (incomingHeader.version != StorageConfig::kPageFormatVersion) {
        return false;
    }

    std::vector<std::uint8_t> incomingPayload(payload_.size(), 0U);
    std::memcpy(incomingPayload.data(), bytes.data() + sizeof(PageHeader), incomingPayload.size());

    if (incomingHeader.checksum != computeChecksum(incomingPayload)) {
        return false;
    }

    header_ = incomingHeader;
    payload_ = std::move(incomingPayload);
    return true;
}

std::uint32_t Page::computeChecksum(const std::vector<std::uint8_t>& payload) {
    std::uint32_t checksum = 0U;
    for (std::uint8_t value : payload) {
        checksum = (checksum * 131U) + static_cast<std::uint32_t>(value);
    }
    return checksum;
}

} // namespace advdb
