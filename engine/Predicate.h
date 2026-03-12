#pragma once

#include <string>
#include <vector>

#include "CatalogTypes.h"
#include "Value.h"

namespace advdb {

enum class CompareOp { EQ, NEQ, LT, LTE, GT, GTE };

// A single column comparison condition: column <op> value.
struct Predicate {
    std::string column;
    CompareOp op{CompareOp::EQ};
    Value value;
};

// Evaluate one predicate against a row.  Returns false if column is not found,
// the row value is null, or the comparison doesn't hold.
inline bool evaluatePredicate(const Predicate& pred,
                               const Row& row,
                               const TableSchema& schema) {
    // Find column index.
    std::size_t colIdx = schema.columns.size(); // sentinel
    for (std::size_t i = 0; i < schema.columns.size(); ++i) {
        if (schema.columns[i].name == pred.column) {
            colIdx = i;
            break;
        }
    }
    if (colIdx == schema.columns.size() || colIdx >= row.size()) {
        return false;
    }

    const Value& rowVal = row[colIdx];
    if (rowVal.isNull()) {
        return false;
    }

    // INT comparison.
    if (rowVal.isInt() && pred.value.isInt()) {
        const int64_t lhs = rowVal.intVal;
        const int64_t rhs = pred.value.intVal;
        switch (pred.op) {
            case CompareOp::EQ:  return lhs == rhs;
            case CompareOp::NEQ: return lhs != rhs;
            case CompareOp::LT:  return lhs <  rhs;
            case CompareOp::LTE: return lhs <= rhs;
            case CompareOp::GT:  return lhs >  rhs;
            case CompareOp::GTE: return lhs >= rhs;
        }
    }

    // String comparison.
    if (rowVal.isString() && pred.value.isString()) {
        const std::string& lhs = rowVal.strVal;
        const std::string& rhs = pred.value.strVal;
        switch (pred.op) {
            case CompareOp::EQ:  return lhs == rhs;
            case CompareOp::NEQ: return lhs != rhs;
            case CompareOp::LT:  return lhs <  rhs;
            case CompareOp::LTE: return lhs <= rhs;
            case CompareOp::GT:  return lhs >  rhs;
            case CompareOp::GTE: return lhs >= rhs;
        }
    }

    return false; // type mismatch
}

// Returns true only if every predicate in 'preds' holds for the row
// (AND semantics).  An empty predicate list matches all rows.
inline bool evaluatePredicates(const std::vector<Predicate>& preds,
                                const Row& row,
                                const TableSchema& schema) {
    for (const Predicate& p : preds) {
        if (!evaluatePredicate(p, row, schema)) {
            return false;
        }
    }
    return true;
}

} // namespace advdb
