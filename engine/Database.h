#pragma once

#include <string>
#include <vector>

#include "CatalogTypes.h"
#include "IndexManager.h"
#include "QueryExecution.h"
#include "TableCatalog.h"
#include "TransactionManager.h"

#include "Predicate.h"
#include "TableHeap.h"
#include "Value.h"

class Database {
public:
    explicit Database(const std::string& dbPath);
    ~Database();

    bool initialize();
    std::string getVersion();

    bool createTable(const advdb::TableSchema& schema, std::string& error);
    bool getTable(const std::string& name, advdb::TableSchema& schema, std::string& error) const;
    std::vector<std::string> listTables() const;

    // Week 9-10: basic CRUD
    bool insertRow(const std::string& tableName, const advdb::Row& row, std::string& error);
    bool selectRows(const std::string& tableName,
                    const std::vector<advdb::Predicate>& predicates,
                    std::vector<advdb::Row>& outRows,
                    std::string& error);

    bool selectRowsProjected(const std::string& tableName,
                            const std::vector<advdb::Predicate>& predicates,
                            const std::vector<std::string>& projectionColumns,
                            std::vector<advdb::Row>& outRows,
                            std::vector<advdb::ColumnDefinition>& outColumns,
                            std::string& error);

    bool createIndex(const std::string& tableName,
                     const std::string& columnName,
                     std::string& error);

    bool beginTransaction(std::string& error);
    bool commitTransaction(std::string& error);
    bool rollbackTransaction(std::string& error);
    bool inTransaction() const;

private:
    static bool isValidIdentifier(const std::string& value);
    static bool validateSchema(const advdb::TableSchema& schema, std::string& error);

    bool insertRowDirect(const std::string& tableName, const advdb::Row& row, std::string& error);

    std::string dbPath_;
    advdb::TableCatalog catalog_;
    advdb::IndexManager indexManager_;
    advdb::TransactionManager transactionManager_;
};