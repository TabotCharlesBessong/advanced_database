#include "Database.h"

#include <fstream>
#include <iostream>
#include <regex>
#include <unordered_set>

Database::Database(const std::string& dbPath)
    : dbPath_(dbPath),
      catalog_(dbPath + ".catalog") {}

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

    std::cout << "Database initialized at: " << dbPath_ << std::endl;
    return true;
}

std::string Database::getVersion() {
    return "0.2.0";
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

    return true;
}

bool Database::selectRows(const std::string& tableName,
                           const std::vector<advdb::Predicate>& predicates,
                           std::vector<advdb::Row>& outRows,
                           std::string& error) {
    advdb::TableSchema schema;
    if (!catalog_.getTable(tableName, schema, error)) {
        return false;
    }

    const std::string heapPath = dbPath_ + "." + tableName + ".heap";
    advdb::TableHeap heap(heapPath);
    if (!heap.open()) {
        // No rows yet (heap file not created).
        outRows.clear();
        return true;
    }

    outRows.clear();
    heap.scanAll(schema, [&](const advdb::RecordId& /*rid*/, const advdb::Row& row) -> bool {
        if (evaluatePredicates(predicates, row, schema)) {
            outRows.push_back(row);
        }
        return true; // continue scanning
    });

    return true;
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