#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace advdb {

enum class ColumnType {
    Int,
    Varchar,
    Text
};

struct ColumnDefinition {
    std::string name;
    ColumnType type;
    std::uint32_t varcharLength;
    bool nullable;
};

struct TableSchema {
    std::string name;
    std::vector<ColumnDefinition> columns;
};

} // namespace advdb
