#pragma once

#include "QueryExecution.h"
#include "Statistics.h"
#include "SqlAst.h"
#include <memory>
#include <vector>

namespace advdb {

// Cost model for query planning
struct CostEstimate {
    double cpuCost;              // CPU cost for processing
    long long ioCount;           // Number of I/O operations
    long long outputRows;        // Estimated output rows
    double totalCost;            // Combined cost estimate

    CostEstimate() : cpuCost(0.0), ioCount(0), outputRows(0), totalCost(0.0) {}
};

// QueryPlanner: Optimizes query execution plans using cost-based approach
class QueryPlanner {
public:
    QueryPlanner(const Statistics& stats) : statistics_(stats) {}

    // Optimize a query plan based on statistics
    // Returns an optimized PlanNode that should produce equivalent results with lower cost
    std::shared_ptr<PlanNode> optimizePlan(const std::shared_ptr<PlanNode>& plan);

    // Estimate the cost of a plan node
    CostEstimate estimateCost(const std::shared_ptr<PlanNode>& plan) const;

    // Optimize join order for a SELECT statement
    // Returns optimized joins vector
    std::vector<SqlJoinClause> optimizeJoinOrder(const std::string& baseTable,
                                                  const std::vector<SqlJoinClause>& joins);

    // Estimate cost of a specific join operation
    CostEstimate estimateJoinCost(const std::string& leftTable,
                                  long long leftRows,
                                  const std::string& rightTable,
                                  long long rightRows) const;

private:
    const Statistics& statistics_;

    // Helper: Estimate cost for a scan operation
    CostEstimate estimateScanCost(const std::string& tableName) const;

    // Helper: Estimate cost for a filter operation
    CostEstimate estimateFilterCost(const CostEstimate& inputCost, double selectivity) const;

    // Helper: Estimate cost for projection
    CostEstimate estimateProjectionCost(const CostEstimate& inputCost) const;

    // Helper: Estimate cost for sort
    CostEstimate estimateSortCost(const CostEstimate& inputCost) const;

    // Helper: Estimate cost for aggregation
    CostEstimate estimateAggregateCost(const CostEstimate& inputCost) const;

    // Find best join order recursively
    std::vector<SqlJoinClause> findBestJoinOrder(
        const std::string& baseTable,
        const std::vector<SqlJoinClause>& remainingJoins,
        std::vector<std::string>& usedTables
    );

    // Cost constants for model tuning
    static constexpr double SCAN_COST_PER_ROW = 1.0;
    static constexpr double FILTER_COST_PER_ROW = 0.5;
    static constexpr double JOIN_COST_PER_COMPARISON = 2.0;
    static constexpr double SORT_COST_PER_ROW_LOG = 3.0;  // n * log(n)
    static constexpr double AGGREGATE_COST_PER_ROW = 0.1;
};

} // namespace advdb
