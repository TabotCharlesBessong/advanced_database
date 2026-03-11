#pragma once

#include <string>
#include <vector>

#include "CatalogTypes.h"
#include "TableCatalog.h"

class Database {
public:
    explicit Database(const std::string& dbPath);
    ~Database();

    bool initialize();
    std::string getVersion();

    bool createTable(const advdb::TableSchema& schema, std::string& error);
    bool getTable(const std::string& name, advdb::TableSchema& schema, std::string& error) const;
    std::vector<std::string> listTables() const;

private:
    static bool isValidIdentifier(const std::string& value);
    static bool validateSchema(const advdb::TableSchema& schema, std::string& error);

    std::string dbPath_;
    advdb::TableCatalog catalog_;
};