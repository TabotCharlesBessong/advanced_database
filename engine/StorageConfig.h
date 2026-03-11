#pragma once

#include <cstdint>

namespace advdb {

struct StorageConfig {
    static constexpr std::uint32_t kDefaultPageSize = 4096;
    static constexpr std::uint16_t kPageFormatVersion = 1;
};

} // namespace advdb
