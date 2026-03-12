#include "TableCatalog.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace advdb {

namespace {

std::vector<std::string> split(const std::string& input, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(input);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        parts.push_back(token);
    }
    return parts;
}

std::string typeToString(ColumnType type) {
    switch (type) {
        case ColumnType::Int:
            return "int";
        case ColumnType::Varchar:
            return "varchar";
        case ColumnType::Text:
            return "text";
    }
    return "unknown";
}

bool stringToType(const std::string& value, ColumnType& outType) {
    if (value == "int") {
        outType = ColumnType::Int;
        return true;
    }
    if (value == "varchar") {
        outType = ColumnType::Varchar;
        return true;
    }
    if (value == "text") {
        outType = ColumnType::Text;
        return true;
    }
    return false;
}

} // namespace

TableCatalog::TableCatalog(std::string catalogPath) : catalogPath_(std::move(catalogPath)), tables_() {}

bool TableCatalog::load(std::string& error) {
    tables_.clear();

    std::ifstream input(catalogPath_);
    if (!input.good()) {
        error.clear();
        return true;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> parts = split(line, '|');
        if (parts.size() != 6U) {
            error = "Malformed catalog line";
            return false;
        }

        const std::string& tableName = parts[0];
        ColumnType type;
        if (!stringToType(parts[2], type)) {
            error = "Unsupported catalog column type";
            return false;
        }

        const std::uint32_t varcharLength = static_cast<std::uint32_t>(std::stoul(parts[3]));
        const bool nullable = parts[4] == "1";
        const std::size_t ordinal = static_cast<std::size_t>(std::stoul(parts[5]));

        TableSchema& table = tables_[tableName];
        table.name = tableName;
        if (table.columns.size() <= ordinal) {
            table.columns.resize(ordinal + 1U);
        }

        table.columns[ordinal] = ColumnDefinition{parts[1], type, varcharLength, nullable};
    }

    return true;
}

bool TableCatalog::persist(std::string& error) const {
    std::ofstream output(catalogPath_, std::ios::trunc);
    if (!output.is_open()) {
        error = "Failed to open catalog for writing";
        return false;
    }

    for (const auto& entry : tables_) {
        const TableSchema& table = entry.second;
        for (std::size_t i = 0; i < table.columns.size(); ++i) {
            const ColumnDefinition& column = table.columns[i];
            output << table.name << '|'
                   << column.name << '|'
                   << typeToString(column.type) << '|'
                   << column.varcharLength << '|'
                   << (column.nullable ? "1" : "0") << '|'
                   << i
                   << '\n';
        }
    }

    if (!output.good()) {
        error = "Failed to write catalog";
        return false;
    }

    return true;
}

bool TableCatalog::createTable(const TableSchema& schema, std::string& error) {
    if (tables_.find(schema.name) != tables_.end()) {
        error = "Table already exists";
        return false;
    }

    tables_[schema.name] = schema;
    return true;
}

bool TableCatalog::getTable(const std::string& name, TableSchema& schema, std::string& error) const {
    auto it = tables_.find(name);
    if (it == tables_.end()) {
        error = "Table not found";
        return false;
    }

    schema = it->second;
    return true;
}

std::vector<std::string> TableCatalog::listTables() const {
    std::vector<std::string> names;
    names.reserve(tables_.size());

    for (const auto& entry : tables_) {
        names.push_back(entry.first);
    }

    std::sort(names.begin(), names.end());
    return names;
}

} // namespace advdb
