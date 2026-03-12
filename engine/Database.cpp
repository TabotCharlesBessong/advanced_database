#include "Database.h"

#include <fstream>
#include <iostream>
#include <regex>
#include <unordered_map>
#include <unordered_set>

Database::Database(const std::string& dbPath)
    : dbPath_(dbPath),
    catalog_(dbPath + ".catalog"),
    indexManager_(dbPath) {}

Database::~Database() {}

bool Database::initialize() {
    std::ofstream dbFile(dbPath_, std::ios::binary | std::ios::app);
    if (!dbFile) {
        std::cerr << "Failed to create database file: " << dbPath_ << std::endl;
        return false;
    }

    dbFile.seekp(0, std::ios::end);
    if (dbFile.tellp() == 0) {
        const char* header = "ADVDB001";
        dbFile.write(header, 8);
    }

    dbFile.close();

    std::string error;
    if (!catalog_.load(error)) {
        std::cerr << "Failed to load table catalog: " << error << std::endl;
        return false;
    }

    if (!catalog_.persist(error)) {
        std::cerr << "Failed to persist table catalog: " << error << std::endl;
        return false;
    }

    if (!indexManager_.initialize(catalog_, error)) {
        std::cerr << "Failed to initialize index manager: " << error << std::endl;
        return false;
    }

    std::cout << "Database initialized at: " << dbPath_ << std::endl;
    return true;
}

std::string Database::getVersion() {
    return "0.3.0";
}

bool Database::createTable(const advdb::TableSchema& schema, std::string& error) {
    if (!validateSchema(schema, error)) {
        return false;
    }

    if (!catalog_.createTable(schema, error)) {
        return false;
    }

    return catalog_.persist(error);
}

bool Database::getTable(const std::string& name, advdb::TableSchema& schema, std::string& error) const {
    return catalog_.getTable(name, schema, error);
}

std::vector<std::string> Database::listTables() const {
    return catalog_.listTables();
}

bool Database::insertRow(const std::string& tableName, const advdb::Row& row, std::string& error) {
    advdb::TableSchema schema;
    if (!catalog_.getTable(tableName, schema, error)) {
        return false;
    }

    if (row.size() != schema.columns.size()) {
        error = "Row has " + std::to_string(row.size()) + " values but table has " +
                std::to_string(schema.columns.size()) + " columns";
        return false;
    }

    // Validate nullability constraints.
    for (std::size_t i = 0u; i < schema.columns.size(); ++i) {
        if (row[i].isNull() && !schema.columns[i].nullable) {
            error = "Column '" + schema.columns[i].name + "' is NOT NULL";
            return false;
        }
    }

    const std::string heapPath = dbPath_ + "." + tableName + ".heap";
    advdb::TableHeap heap(heapPath);
    if (!heap.open()) {
        error = "Failed to open heap file for table: " + tableName;
        return false;
    }

    advdb::RecordId rid;
    if (!heap.insertRow(schema, row, rid)) {
        error = "Failed to insert row into heap";
        return false;
    }

    if (!indexManager_.onRowInserted(tableName, schema, row, error)) {
        return false;
    }

    return true;
}

bool Database::selectRows(const std::string& tableName,
                           const std::vector<advdb::Predicate>& predicates,
                           std::vector<advdb::Row>& outRows,
                           std::string& error) {
    std::vector<advdb::ColumnDefinition> outColumns;
    return selectRowsProjected(tableName, predicates, {}, outRows, outColumns, error);
}

bool Database::selectRowsProjected(const std::string& tableName,
                                   const std::vector<advdb::Predicate>& predicates,
                                   const std::vector<std::string>& projectionColumns,
                                   std::vector<advdb::Row>& outRows,
                                   std::vector<advdb::ColumnDefinition>& outColumns,
                                   std::string& error) {
    advdb::TableSchema schema;
    if (!catalog_.getTable(tableName, schema, error)) {
        return false;
    }

<<<<<<< HEAD
    // Validate projection columns against the schema up front so that unknown
    // column names are rejected even when the heap file does not exist yet.
    std::vector<advdb::ColumnDefinition> projectedColumns;
    if (!projectionColumns.empty()) {
        std::unordered_map<std::string, std::size_t> indexMap;
        for (std::size_t i = 0; i < schema.columns.size(); ++i) {
            indexMap[schema.columns[i].name] = i;
        }
        for (const std::string& colName : projectionColumns) {
            const auto it = indexMap.find(colName);
            if (it == indexMap.end()) {
                error = "Unknown projection column: " + colName;
                return false;
            }
            projectedColumns.push_back(schema.columns[it->second]);
        }
    } else {
        projectedColumns = schema.columns;
=======
    std::vector<std::size_t> projectionIndices;
    if (projectionColumns.empty()) {
        outColumns = schema.columns;
    } else {
        outColumns.clear();
        projectionIndices.reserve(projectionColumns.size());
        for (const std::string& projectionColumn : projectionColumns) {
            std::size_t idx = schema.columns.size();
            for (std::size_t i = 0; i < schema.columns.size(); ++i) {
                if (schema.columns[i].name == projectionColumn) {
                    idx = i;
                    break;
                }
            }
            if (idx == schema.columns.size()) {
                error = "Unknown projection column: " + projectionColumn;
                return false;
            }
            projectionIndices.push_back(idx);
            outColumns.push_back(schema.columns[idx]);
        }
    }

    bool usedIndex = false;
    std::vector<advdb::Row> indexedRows;
    if (!predicates.empty()) {
        if (!indexManager_.tryIndexLookup(tableName, schema, predicates, indexedRows, usedIndex, error)) {
            return false;
        }
    }

    if (usedIndex) {
        outRows.clear();
        for (const advdb::Row& row : indexedRows) {
            if (!advdb::evaluatePredicates(predicates, row, schema)) {
                continue;
            }

            if (projectionColumns.empty()) {
                outRows.push_back(row);
            } else {
                advdb::Row projected;
                projected.reserve(projectionIndices.size());
                for (std::size_t idx : projectionIndices) {
                    if (idx >= row.size()) {
                        error = "Index row projection failed: index out of range";
                        return false;
                    }
                    projected.push_back(row[idx]);
                }
                outRows.push_back(std::move(projected));
            }
        }
        return true;
>>>>>>> da67f51 (Add IndexManager integration to Database class; implement createIndex method and update initialization)
    }

    const std::string heapPath = dbPath_ + "." + tableName + ".heap";
    advdb::TableHeap heap(heapPath);
    if (!heap.open()) {
        // No rows yet (heap file may not exist).
        outRows.clear();
<<<<<<< HEAD
        outColumns = projectedColumns;
=======
>>>>>>> da67f51 (Add IndexManager integration to Database class; implement createIndex method and update initialization)
        return true;
    }

    advdb::ExecutionEngine engine;
    return engine.executeSelect(heap, schema, predicates, projectionColumns, outRows, outColumns, error);
}

bool Database::createIndex(const std::string& tableName,
                           const std::string& columnName,
                           std::string& error) {
    advdb::TableSchema schema;
    if (!catalog_.getTable(tableName, schema, error)) {
        return false;
    }

    const std::string heapPath = dbPath_ + "." + tableName + ".heap";
    return indexManager_.createIndex(tableName, schema, columnName, heapPath, error);
}

bool Database::isValidIdentifier(const std::string& value) {
    static const std::regex kIdentifierPattern("^[A-Za-z_][A-Za-z0-9_]*$");
    return std::regex_match(value, kIdentifierPattern);
}

bool Database::validateSchema(const advdb::TableSchema& schema, std::string& error) {
    if (!isValidIdentifier(schema.name)) {
        error = "Invalid table name";
        return false;
    }

    if (schema.columns.empty()) {
        error = "Table must have at least one column";
        return false;
    }

    std::unordered_set<std::string> names;
    for (const advdb::ColumnDefinition& column : schema.columns) {
        if (!isValidIdentifier(column.name)) {
            error = "Invalid column name: " + column.name;
            return false;
        }

        if (!names.insert(column.name).second) {
            error = "Duplicate column name: " + column.name;
            return false;
        }

        if (column.type == advdb::ColumnType::Varchar && column.varcharLength == 0U) {
            error = "VARCHAR columns must define a positive length";
            return false;
        }

        if (column.type != advdb::ColumnType::Varchar && column.varcharLength != 0U) {
            error = "Only VARCHAR columns can define a length";
            return false;
        }
    }

    return true;
}