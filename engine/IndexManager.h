#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "BTreeIndex.h"
#include "CatalogTypes.h"
#include "Predicate.h"
#include "TableCatalog.h"

namespace advdb {

class IndexManager {
public:
    explicit IndexManager(std::string dbPath);

    bool initialize(const TableCatalog& catalog, std::string& error);

    bool createIndex(const std::string& tableName,
                     const TableSchema& schema,
                     const std::string& columnName,
                     const std::string& heapPath,
                     std::string& error);

    bool onRowInserted(const std::string& tableName,
                       const TableSchema& schema,
                       const Row& row,
                       std::string& error);

    bool tryIndexLookup(const std::string& tableName,
                        const TableSchema& schema,
                        const std::vector<Predicate>& predicates,
                        std::vector<Row>& outRows,
                        bool& usedIndex,
                        std::string& error) const;

private:
    struct IndexMeta {
        std::string tableName;
        std::string columnName;
        IndexKeyType keyType{IndexKeyType::Int};
    };

    bool loadMetadata(std::string& error);
    bool persistMetadata(std::string& error) const;

    static std::string makeIndexKey(const std::string& tableName, const std::string& columnName);
    static std::string makeIndexFilePath(const std::string& dbPath,
                                         const std::string& tableName,
                                         const std::string& columnName);

    static bool findColumn(const TableSchema& schema,
                           const std::string& columnName,
                           std::size_t& outIdx,
                           ColumnType& outType);

    std::string dbPath_;
    std::string metadataPath_;
    std::vector<IndexMeta> metadata_;
    std::unordered_map<std::string, std::unique_ptr<BTreeIndex>> indexes_;
};

} // namespace advdb
