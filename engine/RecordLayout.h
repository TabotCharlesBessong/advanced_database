#pragma once

#include <cstdint>
#include <vector>

#include "CatalogTypes.h"
#include "Value.h"

namespace advdb {

// Serializes and deserializes rows to/from raw bytes.
//
// Binary format:
//   [null_bitmap : ceil(numColumns/8) bytes]  — 1-bit per column, MSB=col0
//   [column data, non-null columns in order]:
//     INT         : 8 bytes little-endian int64_t
//     VARCHAR/TEXT: 4-byte LE uint32_t length, then 'length' UTF-8 bytes
class RecordLayout {
public:
    static std::vector<uint8_t> encode(const TableSchema& schema, const Row& row);
    static bool decode(const TableSchema& schema, const std::vector<uint8_t>& data, Row& row);
};

} // namespace advdb
