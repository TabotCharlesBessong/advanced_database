#include "RecordLayout.h"

#include <cstddef>

namespace advdb {

namespace {

void writeUint32LE(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFFu));
    buf.push_back(static_cast<uint8_t>((val >> 8u) & 0xFFu));
    buf.push_back(static_cast<uint8_t>((val >> 16u) & 0xFFu));
    buf.push_back(static_cast<uint8_t>((val >> 24u) & 0xFFu));
}

void writeInt64LE(std::vector<uint8_t>& buf, int64_t val) {
    const uint64_t bits = static_cast<uint64_t>(val);
    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFFu));
    }
}

uint32_t readUint32LE(const std::vector<uint8_t>& buf, std::size_t offset) {
    return static_cast<uint32_t>(buf[offset]) |
           (static_cast<uint32_t>(buf[offset + 1u]) << 8u) |
           (static_cast<uint32_t>(buf[offset + 2u]) << 16u) |
           (static_cast<uint32_t>(buf[offset + 3u]) << 24u);
}

int64_t readInt64LE(const std::vector<uint8_t>& buf, std::size_t offset) {
    uint64_t bits = 0u;
    for (int i = 0; i < 8; ++i) {
        bits |= static_cast<uint64_t>(buf[offset + static_cast<std::size_t>(i)]) << (i * 8);
    }
    return static_cast<int64_t>(bits);
}

} // namespace

std::vector<uint8_t> RecordLayout::encode(const TableSchema& schema, const Row& row) {
    const std::size_t numCols = schema.columns.size();
    if (row.size() != numCols) {
        return {};
    }

    const std::size_t bitmapBytes = (numCols + 7u) / 8u;
    std::vector<uint8_t> bitmap(bitmapBytes, 0u);
    for (std::size_t i = 0; i < numCols; ++i) {
        if (row[i].isNull()) {
            bitmap[i / 8u] |= static_cast<uint8_t>(1u << (i % 8u));
        }
    }

    std::vector<uint8_t> buf;
    buf.insert(buf.end(), bitmap.begin(), bitmap.end());

    for (std::size_t i = 0; i < numCols; ++i) {
        if (row[i].isNull()) {
            continue;
        }
        if (schema.columns[i].type == ColumnType::Int) {
            writeInt64LE(buf, row[i].intVal);
        } else {
            const std::string& s = row[i].strVal;
            writeUint32LE(buf, static_cast<uint32_t>(s.size()));
            buf.insert(buf.end(), s.begin(), s.end());
        }
    }

    return buf;
}

bool RecordLayout::decode(const TableSchema& schema, const std::vector<uint8_t>& data, Row& row) {
    const std::size_t numCols = schema.columns.size();
    const std::size_t bitmapBytes = (numCols + 7u) / 8u;

    if (data.size() < bitmapBytes) {
        return false;
    }

    row.resize(numCols);
    std::size_t pos = bitmapBytes;

    for (std::size_t i = 0; i < numCols; ++i) {
        const bool isNull = (data[i / 8u] >> (i % 8u)) & 1u;
        if (isNull) {
            row[i] = Value::makeNull();
            continue;
        }

        if (schema.columns[i].type == ColumnType::Int) {
            if (pos + 8u > data.size()) {
                return false;
            }
            row[i] = Value::makeInt(readInt64LE(data, pos));
            pos += 8u;
        } else {
            if (pos + 4u > data.size()) {
                return false;
            }
            const uint32_t len = readUint32LE(data, pos);
            pos += 4u;
            if (pos + len > data.size()) {
                return false;
            }
            row[i] = Value::makeString(
                std::string(data.begin() + static_cast<std::ptrdiff_t>(pos),
                            data.begin() + static_cast<std::ptrdiff_t>(pos + len)));
            pos += len;
        }
    }

    return true;
}

} // namespace advdb
