#pragma once

#include <map>
#include <string>
#include <vector>

namespace advdb {

// Statistics for a single column
struct ColumnStats {
    std::string columnName;
    long long distinctCount{0};
    long long minValue{0};
    long long maxValue{0};
    std::string minString;
    std::string maxString;
    bool isNumeric{false};
};

// Statistics for a table
struct TableStats {
    std::string tableName;
    long long rowCount{0};
    std::vector<ColumnStats> columnStats;
};

// Manages table and column statistics for cost-based optimization
class Statistics {
public:
    Statistics() = default;

    // Update or create table statistics
    void updateTableStats(const TableStats& stats);

    // Get table statistics (returns nullptr if not found)
    const TableStats* getTableStats(const std::string& tableName) const;

    // Estimate selectivity (0.0 to 1.0) for a predicate
    double estimateSelectivity(const std::string& tableName,
                              const std::string& columnName,
                              double value) const;

    // Estimate output row count for a scan with predicates
    long long estimateOutputRows(const std::string& tableName, double selectivity) const;

    // List all tables with statistics
    std::vector<std::string> listTables() const;

private:
    std::map<std::string, TableStats> tableStats_;

    int getColumnIndex(const TableStats& table, const std::string& columnName) const;
};

} // namespace advdb
