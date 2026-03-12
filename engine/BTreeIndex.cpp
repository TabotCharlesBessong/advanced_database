#include "BTreeIndex.h"

#include <fstream>

namespace advdb {

namespace {

static constexpr char kMagic[] = "ADBIDX1";

} // namespace

BTreeIndex::BTreeIndex(std::string indexFilePath, IndexKeyType keyType)
    : indexFilePath_(std::move(indexFilePath)), keyType_(keyType) {}

BTreeIndex::~BTreeIndex() {
    std::string ignored;
    close(ignored);
}

bool BTreeIndex::open(std::string& error) {
    if (opened_) {
        return true;
    }
    entries_.clear();
    if (!loadFromDisk(error)) {
        return false;
    }
    opened_ = true;
    dirty_ = false;
    return true;
}

bool BTreeIndex::close(std::string& error) {
    if (!opened_) {
        return true;
    }
    if (dirty_ && !flushToDisk(error)) {
        return false;
    }
    opened_ = false;
    dirty_ = false;
    return true;
}

bool BTreeIndex::insertEntry(const Value& key, const Row& row, std::string& error) {
    if (!opened_) {
        error = "Index is not open";
        return false;
    }

    Key internalKey;
    if (!toKey(key, keyType_, internalKey, error)) {
        return false;
    }

    entries_[internalKey].push_back(row);
    dirty_ = true;
    return true;
}

bool BTreeIndex::pointLookup(const Value& key, std::vector<Row>& outRows, std::string& error) const {
    outRows.clear();
    if (!opened_) {
        error = "Index is not open";
        return false;
    }

    Key internalKey;
    if (!toKey(key, keyType_, internalKey, error)) {
        return false;
    }

    const auto it = entries_.find(internalKey);
    if (it == entries_.end()) {
        return true;
    }

    outRows = it->second;
    return true;
}

bool BTreeIndex::rangeLookup(const Value* minKey,
                             const Value* maxKey,
                             bool includeMin,
                             bool includeMax,
                             std::vector<Row>& outRows,
                             std::string& error) const {
    outRows.clear();
    if (!opened_) {
        error = "Index is not open";
        return false;
    }

    Key minInternal;
    Key maxInternal;
    if (minKey && !toKey(*minKey, keyType_, minInternal, error)) {
        return false;
    }
    if (maxKey && !toKey(*maxKey, keyType_, maxInternal, error)) {
        return false;
    }

    auto it = minKey ? entries_.lower_bound(minInternal) : entries_.begin();
    for (; it != entries_.end(); ++it) {
        if (maxKey) {
            const bool gtMax = maxInternal < it->first;
            const bool eqMax = !(it->first < maxInternal) && !(maxInternal < it->first);
            if (gtMax || (!includeMax && eqMax)) {
                break;
            }
        }

        if (minKey) {
            const bool eqMin = !(it->first < minInternal) && !(minInternal < it->first);
            if (!includeMin && eqMin) {
                continue;
            }
        }

        outRows.insert(outRows.end(), it->second.begin(), it->second.end());
    }

    return true;
}

bool BTreeIndex::loadFromDisk(std::string& error) {
    std::ifstream in(indexFilePath_, std::ios::binary);
    if (!in) {
        return true;
    }

    char magic[sizeof(kMagic)]{};
    in.read(magic, sizeof(kMagic) - 1);
    if (!in) {
        error = "Failed reading index header";
        return false;
    }

    if (std::string(magic) != kMagic) {
        error = "Invalid index file format";
        return false;
    }

    uint8_t fileType = 0;
    in.read(reinterpret_cast<char*>(&fileType), sizeof(fileType));
    if (!in) {
        error = "Failed reading index key type";
        return false;
    }
    if (fileType != static_cast<uint8_t>(keyType_)) {
        error = "Index key type mismatch";
        return false;
    }

    uint64_t keyCount = 0;
    in.read(reinterpret_cast<char*>(&keyCount), sizeof(keyCount));
    if (!in) {
        error = "Failed reading key count";
        return false;
    }

    for (uint64_t i = 0; i < keyCount; ++i) {
        Value keyValue;
        if (!readValue(in, keyValue)) {
            error = "Failed reading index key";
            return false;
        }

        Key key;
        if (!toKey(keyValue, keyType_, key, error)) {
            return false;
        }

        uint64_t rowCount = 0;
        in.read(reinterpret_cast<char*>(&rowCount), sizeof(rowCount));
        if (!in) {
            error = "Failed reading row count";
            return false;
        }

        auto& bucket = entries_[key];
        bucket.reserve(static_cast<std::size_t>(rowCount));
        for (uint64_t r = 0; r < rowCount; ++r) {
            Row row;
            if (!readRow(in, row)) {
                error = "Failed reading index row";
                return false;
            }
            bucket.push_back(std::move(row));
        }
    }

    return true;
}

bool BTreeIndex::flushToDisk(std::string& error) const {
    std::ofstream out(indexFilePath_, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "Failed opening index file for write";
        return false;
    }

    out.write(kMagic, sizeof(kMagic) - 1);

    const uint8_t keyType = static_cast<uint8_t>(keyType_);
    out.write(reinterpret_cast<const char*>(&keyType), sizeof(keyType));

    const uint64_t keyCount = static_cast<uint64_t>(entries_.size());
    out.write(reinterpret_cast<const char*>(&keyCount), sizeof(keyCount));

    for (const auto& entry : entries_) {
        Value keyValue;
        if (entry.first.kind == Value::Kind::Int) {
            keyValue = Value::makeInt(entry.first.intVal);
        } else {
            keyValue = Value::makeString(entry.first.strVal);
        }

        if (!writeValue(out, keyValue)) {
            error = "Failed writing index key";
            return false;
        }

        const uint64_t rowCount = static_cast<uint64_t>(entry.second.size());
        out.write(reinterpret_cast<const char*>(&rowCount), sizeof(rowCount));

        for (const Row& row : entry.second) {
            if (!writeRow(out, row)) {
                error = "Failed writing index row";
                return false;
            }
        }
    }

    if (!out.good()) {
        error = "Failed flushing index file";
        return false;
    }

    return true;
}

bool BTreeIndex::toKey(const Value& value, IndexKeyType expectedType, Key& outKey, std::string& error) {
    if (expectedType == IndexKeyType::Int) {
        if (!value.isInt()) {
            error = "Index key type mismatch: expected INT";
            return false;
        }
        outKey.kind = Value::Kind::Int;
        outKey.intVal = value.intVal;
        outKey.strVal.clear();
        return true;
    }

    if (!value.isString()) {
        error = "Index key type mismatch: expected STRING";
        return false;
    }

    outKey.kind = Value::Kind::String;
    outKey.intVal = 0;
    outKey.strVal = value.strVal;
    return true;
}

bool BTreeIndex::writeValue(std::ofstream& out, const Value& value) {
    const uint8_t kind = static_cast<uint8_t>(value.kind);
    out.write(reinterpret_cast<const char*>(&kind), sizeof(kind));

    if (value.isInt()) {
        out.write(reinterpret_cast<const char*>(&value.intVal), sizeof(value.intVal));
    } else if (value.isString()) {
        const uint32_t len = static_cast<uint32_t>(value.strVal.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        if (len > 0) {
            out.write(value.strVal.data(), len);
        }
    }

    return out.good();
}

bool BTreeIndex::readValue(std::ifstream& in, Value& value) {
    uint8_t kind = 0;
    in.read(reinterpret_cast<char*>(&kind), sizeof(kind));
    if (!in) {
        return false;
    }

    value = Value::makeNull();
    const auto valueKind = static_cast<Value::Kind>(kind);
    if (valueKind == Value::Kind::Int) {
        int64_t v = 0;
        in.read(reinterpret_cast<char*>(&v), sizeof(v));
        if (!in) {
            return false;
        }
        value = Value::makeInt(v);
    } else if (valueKind == Value::Kind::String) {
        uint32_t len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (!in) {
            return false;
        }
        std::string s(len, '\0');
        if (len > 0) {
            in.read(&s[0], len);
            if (!in) {
                return false;
            }
        }
        value = Value::makeString(std::move(s));
    }

    return true;
}

bool BTreeIndex::writeRow(std::ofstream& out, const Row& row) {
    const uint32_t columns = static_cast<uint32_t>(row.size());
    out.write(reinterpret_cast<const char*>(&columns), sizeof(columns));
    for (const Value& value : row) {
        if (!writeValue(out, value)) {
            return false;
        }
    }
    return out.good();
}

bool BTreeIndex::readRow(std::ifstream& in, Row& row) {
    uint32_t columns = 0;
    in.read(reinterpret_cast<char*>(&columns), sizeof(columns));
    if (!in) {
        return false;
    }

    row.clear();
    row.reserve(columns);
    for (uint32_t i = 0; i < columns; ++i) {
        Value value;
        if (!readValue(in, value)) {
            return false;
        }
        row.push_back(std::move(value));
    }

    return true;
}

} // namespace advdb
