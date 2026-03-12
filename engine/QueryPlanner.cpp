#include "QueryPlanner.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace advdb {

std::shared_ptr<PlanNode> QueryPlanner::optimizePlan(const std::shared_ptr<PlanNode>& plan) {
    // For MVP phase, perform basic optimizations:
    // 1. Push filters down (not implemented yet, as SqlParser doesn't build filter nodes)
    // 2. Optimize join order (would require analyzing join graph)
    // 3. Estimate and log costs for analysis
    
    // Currently, return the original plan as query execution handles plan construction
    // Full push-down reordering would require plan traversal and reconstruction
    return plan;
}

CostEstimate QueryPlanner::estimateCost(const std::shared_ptr<PlanNode>& plan) const {
    CostEstimate cost;

    if (!plan) {
        return cost;
    }

    // Estimate based on plan node type
    switch (plan->type) {
        case PlanNodeType::Scan: {
            // Scan cost is based on table size
            // detail field contains table name
            cost = estimateScanCost(plan->detail);
            break;
        }

        case PlanNodeType::Filter: {
            // Filter cost is input cost + filter evaluation
            if (!plan->children.empty()) {
                auto childPtr = std::make_shared<PlanNode>(plan->children[0]);
                cost = estimateCost(childPtr);
                // Assume 50% selectivity for unknown predicates
                cost = estimateFilterCost(cost, 0.5);
            }
            break;
        }

        case PlanNodeType::Project: {
            // Projection cost is input cost + projection overhead
            if (!plan->children.empty()) {
                auto childPtr = std::make_shared<PlanNode>(plan->children[0]);
                cost = estimateCost(childPtr);
                cost = estimateProjectionCost(cost);
            }
            break;
        }

        case PlanNodeType::Join: {
            // Join cost combines left and right inputs via nested loop join model
            if (plan->children.size() == 2) {
                auto leftPtr = std::make_shared<PlanNode>(plan->children[0]);
                auto rightPtr = std::make_shared<PlanNode>(plan->children[1]);
                CostEstimate leftCost = estimateCost(leftPtr);
                CostEstimate rightCost = estimateCost(rightPtr);
                cost = estimateJoinCost(
                    leftPtr->detail, leftCost.outputRows,
                    rightPtr->detail, rightCost.outputRows);
            }
            break;
        }

        case PlanNodeType::Sort: {
            // Sort cost is O(n log n)
            if (!plan->children.empty()) {
                auto childPtr = std::make_shared<PlanNode>(plan->children[0]);
                cost = estimateCost(childPtr);
                cost = estimateSortCost(cost);
            }
            break;
        }

        case PlanNodeType::GroupBy: {
            // Groupby cost includes scan + grouping
            if (!plan->children.empty()) {
                auto childPtr = std::make_shared<PlanNode>(plan->children[0]);
                cost = estimateCost(childPtr);
                cost.cpuCost *= 1.5;  // Grouping overhead
                cost.totalCost = cost.cpuCost + (cost.ioCount * 10.0);
            }
            break;
        }

        case PlanNodeType::Aggregate: {
            // Aggregate cost includes scan + aggregation
            if (!plan->children.empty()) {
                auto childPtr = std::make_shared<PlanNode>(plan->children[0]);
                cost = estimateCost(childPtr);
                cost = estimateAggregateCost(cost);
            }
            break;
        }
    }

    return cost;
}

std::vector<SqlJoinClause> QueryPlanner::optimizeJoinOrder(const std::string& baseTable,
                                                             const std::vector<SqlJoinClause>& joins) {
    if (joins.empty()) {
        return joins;
    }

    // Use dynamic programming-like approach to find best join order
    std::vector<std::string> usedTables;
    usedTables.push_back(baseTable);

    return findBestJoinOrder(baseTable, joins, usedTables);
}

CostEstimate QueryPlanner::estimateJoinCost(const std::string& leftTable,
                                             long long leftRows,
                                             const std::string& rightTable,
                                             long long rightRows) const {
    CostEstimate cost;

    // Nested loop join: cost = left_rows * (scan_right + join_condition)
    cost.outputRows = leftRows * rightRows;  // Assuming Cartesian product, filtered later
    cost.cpuCost = (leftRows * JOIN_COST_PER_COMPARISON) + (leftRows * rightRows * 0.1);
    cost.ioCount = leftRows + rightRows;
    cost.totalCost = cost.cpuCost + (cost.ioCount * 10.0);

    return cost;
}

CostEstimate QueryPlanner::estimateScanCost(const std::string& tableName) const {
    CostEstimate cost;

    const TableStats* tableStats = statistics_.getTableStats(tableName);
    if (tableStats) {
        cost.outputRows = tableStats->rowCount;
        cost.ioCount = (tableStats->rowCount + 999) / 1000;  // Assume 1000 rows per I/O block
        cost.cpuCost = tableStats->rowCount * SCAN_COST_PER_ROW;
    } else {
        // No statistics, assume 1000 rows
        cost.outputRows = 1000;
        cost.ioCount = 10;
        cost.cpuCost = 1000 * SCAN_COST_PER_ROW;
    }

    cost.totalCost = cost.cpuCost + (cost.ioCount * 10.0);
    return cost;
}

CostEstimate QueryPlanner::estimateFilterCost(const CostEstimate& inputCost, double selectivity) const {
    CostEstimate cost = inputCost;
    cost.outputRows = static_cast<long long>(inputCost.outputRows * selectivity);
    cost.cpuCost += inputCost.outputRows * FILTER_COST_PER_ROW;
    cost.totalCost = cost.cpuCost + (cost.ioCount * 10.0);
    return cost;
}

CostEstimate QueryPlanner::estimateProjectionCost(const CostEstimate& inputCost) const {
    CostEstimate cost = inputCost;
    // Projection is relatively cheap
    cost.cpuCost += inputCost.outputRows * 0.1;
    cost.totalCost = cost.cpuCost + (cost.ioCount * 10.0);
    return cost;
}

CostEstimate QueryPlanner::estimateSortCost(const CostEstimate& inputCost) const {
    CostEstimate cost = inputCost;
    // O(n log n) complexity
    if (inputCost.outputRows > 0) {
        double logN = std::log(static_cast<double>(inputCost.outputRows));
        cost.cpuCost += inputCost.outputRows * logN * SORT_COST_PER_ROW_LOG;
    }
    cost.totalCost = cost.cpuCost + (cost.ioCount * 10.0);
    return cost;
}

CostEstimate QueryPlanner::estimateAggregateCost(const CostEstimate& inputCost) const {
    CostEstimate cost = inputCost;
    cost.cpuCost += inputCost.outputRows * AGGREGATE_COST_PER_ROW;
    cost.outputRows = 1;  // Aggregate produces single row (or group rows in GROUP BY)
    cost.totalCost = cost.cpuCost + (cost.ioCount * 10.0);
    return cost;
}

std::vector<SqlJoinClause> QueryPlanner::findBestJoinOrder(
    const std::string& baseTable,
    const std::vector<SqlJoinClause>& remainingJoins,
    std::vector<std::string>& usedTables) {
    
    // Base case: no more joins
    if (remainingJoins.empty()) {
        return {};
    }

    // Pick the next join that produces the lowest cost
    // For MVP, use greedy approach: smallest table first (or best selectivity)
    int bestIndex = 0;
    double bestCost = std::numeric_limits<double>::max();

    for (std::size_t i = 0; i < remainingJoins.size(); ++i) {
        const SqlJoinClause& join = remainingJoins[i];

        // Check if this join's left (driving) table is already in usedTables
        bool leftTableUsed = std::find(usedTables.begin(), usedTables.end(), join.leftTable) != usedTables.end();

        if (!leftTableUsed) {
            continue;  // Skip joins that cannot be performed yet
        }

        // Estimate cost of this join
        const TableStats* rightStats = statistics_.getTableStats(join.joinTable);
        long long rightRows = rightStats ? rightStats->rowCount : 1000;
        long long leftRows = 1000;  // Placeholder; would track cumulative rows

        CostEstimate joinCost = estimateJoinCost(baseTable, leftRows, join.joinTable, rightRows);

        if (joinCost.totalCost < bestCost) {
            bestCost = joinCost.totalCost;
            bestIndex = i;
        }
    }

    // Add the chosen join to result
    std::vector<SqlJoinClause> result;
    result.push_back(remainingJoins[bestIndex]);
    usedTables.push_back(remainingJoins[bestIndex].joinTable);

    // Recursively optimize remaining joins
    std::vector<SqlJoinClause> remaining(remainingJoins.begin(), remainingJoins.end());
    remaining.erase(remaining.begin() + bestIndex);

    auto restResult = findBestJoinOrder(baseTable, remaining, usedTables);
    result.insert(result.end(), restResult.begin(), restResult.end());

    return result;
}

} // namespace advdb
