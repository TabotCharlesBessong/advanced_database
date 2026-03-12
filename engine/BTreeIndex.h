#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Value.h"

namespace advdb {

enum class IndexKeyType {
    Int,
    String
};

class BTreeIndex {
public:
    BTreeIndex(std::string indexFilePath, IndexKeyType keyType);
    ~BTreeIndex();

    bool open(std::string& error);
    bool close(std::string& error);

    bool insertEntry(const Value& key, const Row& row, std::string& error);
    bool pointLookup(const Value& key, std::vector<Row>& outRows, std::string& error) const;

    bool rangeLookup(const Value* minKey,
                     const Value* maxKey,
                     bool includeMin,
                     bool includeMax,
                     std::vector<Row>& outRows,
                     std::string& error) const;

private:
    struct Key {
        Value::Kind kind{Value::Kind::Null};
        int64_t intVal{0};
        std::string strVal;

        bool operator<(const Key& other) const {
            if (kind != other.kind) {
                return kind < other.kind;
            }
            if (kind == Value::Kind::Int) {
                return intVal < other.intVal;
            }
            return strVal < other.strVal;
        }
    };

    bool loadFromDisk(std::string& error);
    bool flushToDisk(std::string& error) const;

    static bool toKey(const Value& value, IndexKeyType expectedType, Key& outKey, std::string& error);

    static bool writeValue(std::ofstream& out, const Value& value);
    static bool readValue(std::ifstream& in, Value& value);
    static bool writeRow(std::ofstream& out, const Row& row);
    static bool readRow(std::ifstream& in, Row& row);

    std::string indexFilePath_;
    IndexKeyType keyType_;
    bool opened_{false};
    bool dirty_{false};
    std::map<Key, std::vector<Row>> entries_;
};

} // namespace advdb
