#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "CatalogTypes.h"

namespace advdb {

class TableCatalog {
public:
    explicit TableCatalog(std::string catalogPath);

    bool load(std::string& error);
    bool persist(std::string& error) const;

    bool createTable(const TableSchema& schema, std::string& error);
    bool getTable(const std::string& name, TableSchema& schema, std::string& error) const;
    std::vector<std::string> listTables() const;

private:
    std::string catalogPath_;
    std::unordered_map<std::string, TableSchema> tables_;
};

} // namespace advdb
