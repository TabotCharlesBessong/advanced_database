#include "Statistics.h"

namespace advdb {

void Statistics::updateTableStats(const TableStats& stats) {
    tableStats_[stats.tableName] = stats;
}

const TableStats* Statistics::getTableStats(const std::string& tableName) const {
    const auto it = tableStats_.find(tableName);
    if (it == tableStats_.end()) {
        return nullptr;
    }
    return &it->second;
}

double Statistics::estimateSelectivity(const std::string& tableName,
                                       const std::string& columnName,
                                       double value) const {
    const TableStats* tableStats = getTableStats(tableName);
    if (!tableStats) {
        // No stats available, assume 10% selectivity (conservative estimate)
        return 0.1;
    }

    const int colIdx = getColumnIndex(*tableStats, columnName);
    if (colIdx < 0 || colIdx >= static_cast<int>(tableStats->columnStats.size())) {
        return 0.1;  // Unknown column, use conservative estimate
    }

    const ColumnStats& colStats = tableStats->columnStats[colIdx];

    if (colStats.isNumeric && colStats.distinctCount > 0) {
        // For numeric columns, use value distribution estimate
        // Assume uniform distribution
        return 1.0 / static_cast<double>(colStats.distinctCount);
    }

    if (colStats.distinctCount > 0) {
        // For string columns, assume equality filter
        return 1.0 / static_cast<double>(colStats.distinctCount);
    }

    return 0.1;  // Fallback conservative estimate
}

long long Statistics::estimateOutputRows(const std::string& tableName, double selectivity) const {
    const TableStats* tableStats = getTableStats(tableName);
    if (!tableStats) {
        // No stats, assume 1000 rows (default estimate)
        return static_cast<long long>(1000 * selectivity);
    }

    return static_cast<long long>(tableStats->rowCount * selectivity);
}

std::vector<std::string> Statistics::listTables() const {
    std::vector<std::string> tables;
    for (const auto& entry : tableStats_) {
        tables.push_back(entry.first);
    }
    return tables;
}

int Statistics::getColumnIndex(const TableStats& table, const std::string& columnName) const {
    for (std::size_t i = 0; i < table.columnStats.size(); ++i) {
        if (table.columnStats[i].columnName == columnName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace advdb
