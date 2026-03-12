#include "IndexManager.h"

#include <fstream>

#include "TableHeap.h"

namespace advdb {

IndexManager::IndexManager(std::string dbPath)
    : dbPath_(std::move(dbPath)),
      metadataPath_(dbPath_ + ".indexes.meta") {}

bool IndexManager::initialize(const TableCatalog& catalog, std::string& error) {
    metadata_.clear();
    indexes_.clear();

    if (!loadMetadata(error)) {
        return false;
    }

    for (const IndexMeta& meta : metadata_) {
        TableSchema schema;
        if (!catalog.getTable(meta.tableName, schema, error)) {
            return false;
        }

        std::size_t colIdx = 0;
        ColumnType colType = ColumnType::Int;
        if (!findColumn(schema, meta.columnName, colIdx, colType)) {
            error = "Indexed column not found in schema: " + meta.tableName + "." + meta.columnName;
            return false;
        }

        const std::string indexKey = makeIndexKey(meta.tableName, meta.columnName);
        auto index = std::make_unique<BTreeIndex>(
            makeIndexFilePath(dbPath_, meta.tableName, meta.columnName),
            meta.keyType);

        if (!index->open(error)) {
            return false;
        }
        indexes_[indexKey] = std::move(index);
    }

    return true;
}

bool IndexManager::createIndex(const std::string& tableName,
                               const TableSchema& schema,
                               const std::string& columnName,
                               const std::string& heapPath,
                               std::string& error) {
    std::size_t colIdx = 0;
    ColumnType colType = ColumnType::Int;
    if (!findColumn(schema, columnName, colIdx, colType)) {
        error = "Column not found: " + columnName;
        return false;
    }

    const std::string indexKey = makeIndexKey(tableName, columnName);
    if (indexes_.find(indexKey) != indexes_.end()) {
        error = "Index already exists on " + tableName + "." + columnName;
        return false;
    }

    const IndexKeyType keyType = (colType == ColumnType::Int) ? IndexKeyType::Int : IndexKeyType::String;
    auto index = std::make_unique<BTreeIndex>(makeIndexFilePath(dbPath_, tableName, columnName), keyType);
    if (!index->open(error)) {
        return false;
    }

    TableHeap heap(heapPath);
    if (heap.open()) {
        if (!heap.scanAll(schema, [&](const RecordId&, const Row& row) {
                if (colIdx >= row.size() || row[colIdx].isNull()) {
                    return true;
                }
                std::string localError;
                if (!index->insertEntry(row[colIdx], row, localError)) {
                    error = localError;
                    return false;
                }
                return true;
            })) {
            if (error.empty()) {
                error = "Failed scanning heap while creating index";
            }
            return false;
        }
    }

    if (!index->close(error)) {
        return false;
    }
    if (!index->open(error)) {
        return false;
    }

    metadata_.push_back(IndexMeta{tableName, columnName, keyType});
    if (!persistMetadata(error)) {
        return false;
    }

    indexes_[indexKey] = std::move(index);
    return true;
}

bool IndexManager::onRowInserted(const std::string& tableName,
                                 const TableSchema& schema,
                                 const Row& row,
                                 std::string& error) {
    for (const IndexMeta& meta : metadata_) {
        if (meta.tableName != tableName) {
            continue;
        }

        const std::string indexKey = makeIndexKey(meta.tableName, meta.columnName);
        auto it = indexes_.find(indexKey);
        if (it == indexes_.end()) {
            continue;
        }

        std::size_t colIdx = 0;
        ColumnType colType = ColumnType::Int;
        if (!findColumn(schema, meta.columnName, colIdx, colType)) {
            error = "Indexed column not found during insert: " + meta.columnName;
            return false;
        }

        if (colIdx >= row.size() || row[colIdx].isNull()) {
            continue;
        }

        if (!it->second->insertEntry(row[colIdx], row, error)) {
            return false;
        }

        if (!it->second->close(error)) {
            return false;
        }
        if (!it->second->open(error)) {
            return false;
        }
    }

    return true;
}

bool IndexManager::tryIndexLookup(const std::string& tableName,
                                  const TableSchema& schema,
                                  const std::vector<Predicate>& predicates,
                                  std::vector<Row>& outRows,
                                  bool& usedIndex,
                                  std::string& error) const {
    usedIndex = false;
    outRows.clear();

    for (const Predicate& predicate : predicates) {
        const std::string indexKey = makeIndexKey(tableName, predicate.column);
        auto it = indexes_.find(indexKey);
        if (it == indexes_.end()) {
            continue;
        }

        std::size_t colIdx = 0;
        ColumnType colType = ColumnType::Int;
        if (!findColumn(schema, predicate.column, colIdx, colType)) {
            continue;
        }

        usedIndex = true;
        if (predicate.op == CompareOp::EQ) {
            return it->second->pointLookup(predicate.value, outRows, error);
        }

        if (predicate.op == CompareOp::LT || predicate.op == CompareOp::LTE) {
            const bool includeMax = (predicate.op == CompareOp::LTE);
            return it->second->rangeLookup(nullptr, &predicate.value, true, includeMax, outRows, error);
        }

        if (predicate.op == CompareOp::GT || predicate.op == CompareOp::GTE) {
            const bool includeMin = (predicate.op == CompareOp::GTE);
            return it->second->rangeLookup(&predicate.value, nullptr, includeMin, true, outRows, error);
        }

        usedIndex = false;
        outRows.clear();
    }

    return true;
}

bool IndexManager::loadMetadata(std::string& error) {
    std::ifstream in(metadataPath_);
    if (!in) {
        return true;
    }

    std::string table;
    std::string column;
    int keyTypeInt = 0;
    while (in >> table >> column >> keyTypeInt) {
        metadata_.push_back(IndexMeta{table, column, static_cast<IndexKeyType>(keyTypeInt)});
    }

    if (!in.eof() && in.fail()) {
        error = "Failed reading index metadata";
        return false;
    }

    return true;
}

bool IndexManager::persistMetadata(std::string& error) const {
    std::ofstream out(metadataPath_, std::ios::trunc);
    if (!out) {
        error = "Failed writing index metadata";
        return false;
    }

    for (const IndexMeta& meta : metadata_) {
        out << meta.tableName << ' ' << meta.columnName << ' ' << static_cast<int>(meta.keyType) << '\n';
    }

    if (!out.good()) {
        error = "Failed flushing index metadata";
        return false;
    }

    return true;
}

std::string IndexManager::makeIndexKey(const std::string& tableName, const std::string& columnName) {
    return tableName + "." + columnName;
}

std::string IndexManager::makeIndexFilePath(const std::string& dbPath,
                                            const std::string& tableName,
                                            const std::string& columnName) {
    return dbPath + "." + tableName + "." + columnName + ".idx";
}

bool IndexManager::findColumn(const TableSchema& schema,
                              const std::string& columnName,
                              std::size_t& outIdx,
                              ColumnType& outType) {
    for (std::size_t i = 0; i < schema.columns.size(); ++i) {
        if (schema.columns[i].name == columnName) {
            outIdx = i;
            outType = schema.columns[i].type;
            return true;
        }
    }
    return false;
}

} // namespace advdb
