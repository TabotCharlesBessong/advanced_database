#include "QueryExecution.h"

#include <unordered_map>
#include <utility>

namespace advdb {

ScanOperator::ScanOperator(TableHeap& heap, const TableSchema& schema)
    : heap_(heap), schema_(schema) {}

IndexScanOperator::IndexScanOperator(std::vector<Row> rows)
    : rows_(std::move(rows)) {}

bool ScanOperator::open(std::string& error) {
    rows_.clear();
    index_ = 0;

    if (!heap_.scanAll(schema_, [&](const RecordId&, const Row& row) {
            rows_.push_back(row);
            return true;
        })) {
        error = "Table scan failed";
        return false;
    }

    return true;
}

bool ScanOperator::next(Row& outRow, std::string&) {
    if (index_ >= rows_.size()) {
        return false;
    }

    outRow = rows_[index_++];
    return true;
}

void ScanOperator::close() {
    rows_.clear();
    index_ = 0;
}

bool IndexScanOperator::open(std::string&) {
    index_ = 0;
    return true;
}

bool IndexScanOperator::next(Row& outRow, std::string&) {
    if (index_ >= rows_.size()) {
        return false;
    }

    outRow = rows_[index_++];
    return true;
}

void IndexScanOperator::close() {
    index_ = 0;
}

FilterOperator::FilterOperator(std::unique_ptr<IOperator> child,
                               const TableSchema& schema,
                               std::vector<Predicate> predicates)
    : child_(std::move(child)), schema_(schema), predicates_(std::move(predicates)) {}

bool FilterOperator::open(std::string& error) {
    return child_->open(error);
}

bool FilterOperator::next(Row& outRow, std::string& error) {
    Row row;
    while (child_->next(row, error)) {
        if (evaluatePredicates(predicates_, row, schema_)) {
            outRow = std::move(row);
            return true;
        }
    }
    return false;
}

void FilterOperator::close() {
    child_->close();
}

ProjectOperator::ProjectOperator(std::unique_ptr<IOperator> child,
                                 const TableSchema& inputSchema,
                                 std::vector<std::string> projectionColumns,
                                 std::vector<std::size_t> projectionIndices)
    : child_(std::move(child)),
      inputSchema_(inputSchema),
      projectionColumns_(std::move(projectionColumns)),
      projectionIndices_(std::move(projectionIndices)) {}

bool ProjectOperator::open(std::string& error) {
    return child_->open(error);
}

bool ProjectOperator::next(Row& outRow, std::string& error) {
    Row inputRow;
    if (!child_->next(inputRow, error)) {
        return false;
    }

    outRow.clear();
    outRow.reserve(projectionIndices_.size());
    for (std::size_t index : projectionIndices_) {
        if (index >= inputRow.size()) {
            error = "Projection index out of range";
            return false;
        }
        outRow.push_back(inputRow[index]);
    }

    return true;
}

void ProjectOperator::close() {
    child_->close();
}

// Week 15-16: JoinOperator - nested loop join
JoinOperator::JoinOperator(std::unique_ptr<IOperator> leftChild,
                           std::unique_ptr<IOperator> rightChild,
                           const TableSchema& leftSchema,
                           const TableSchema& rightSchema,
                           const std::string& leftColumn,
                           const std::string& rightColumn,
                           JoinType joinType)
    : leftChild_(std::move(leftChild)),
      rightChild_(std::move(rightChild)),
      leftSchema_(leftSchema),
      rightSchema_(rightSchema),
      leftColumn_(leftColumn),
      rightColumn_(rightColumn),
      joinType_(joinType) {}

bool JoinOperator::open(std::string& error) {
    if (!leftChild_->open(error)) {
        return false;
    }
    
    Row leftRow;
    while (leftChild_->next(leftRow, error)) {
        leftRows_.push_back(leftRow);
    }
    leftChild_->close();

    if (!rightChild_->open(error)) {
        return false;
    }
    
    Row rightRow;
    while (rightChild_->next(rightRow, error)) {
        rightRows_.push_back(rightRow);
    }
    rightChild_->close();
    
    rightRowsLoaded_ = true;
    leftIndex_ = 0;
    rightIndex_ = 0;
    
    return true;
}

bool JoinOperator::next(Row& outRow, std::string& error) {
    if (!rightRowsLoaded_ || leftRows_.empty() || rightRows_.empty()) {
        return false;
    }

    // Find left column index
    std::size_t leftColIdx = leftSchema_.columns.size();
    for (std::size_t i = 0; i < leftSchema_.columns.size(); ++i) {
        if (leftSchema_.columns[i].name == leftColumn_) {
            leftColIdx = i;
            break;
        }
    }

    // Find right column index
    std::size_t rightColIdx = rightSchema_.columns.size();
    for (std::size_t i = 0; i < rightSchema_.columns.size(); ++i) {
        if (rightSchema_.columns[i].name == rightColumn_) {
            rightColIdx = i;
            break;
        }
    }

    if (leftColIdx == leftSchema_.columns.size() || rightColIdx == rightSchema_.columns.size()) {
        error = "Join column not found";
        return false;
    }

    while (leftIndex_ < leftRows_.size()) {
        const Row& leftRow = leftRows_[leftIndex_];
        
        for (rightIndex_ = 0; rightIndex_ < rightRows_.size(); ++rightIndex_) {
            const Row& rightRow = rightRows_[rightIndex_];
            
            // Check join condition
            const Value& leftVal = leftRow[leftColIdx];
            const Value& rightVal = rightRow[rightColIdx];
            
            bool matches = (leftVal.isNull() && rightVal.isNull()) ||
                          (!leftVal.isNull() && !rightVal.isNull() &&
                           ((leftVal.isInt() && rightVal.isInt() &&
                             leftVal.intVal == rightVal.intVal) ||
                            (leftVal.isString() && rightVal.isString() &&
                             leftVal.strVal == rightVal.strVal)));
            
            if (matches) {
                outRow.clear();
                outRow.insert(outRow.end(), leftRow.begin(), leftRow.end());
                outRow.insert(outRow.end(), rightRow.begin(), rightRow.end());
                ++rightIndex_;
                return true;
            }
        }
        ++leftIndex_;
    }
    
    return false;
}

void JoinOperator::close() {
    leftRows_.clear();
    rightRows_.clear();
    leftIndex_ = 0;
    rightIndex_ = 0;
}

// Week 15-16: AggregateOperator
AggregateOperator::AggregateOperator(std::unique_ptr<IOperator> child,
                                     const TableSchema& schema,
                                     AggFunc func,
                                     const std::string& column)
    : child_(std::move(child)), schema_(schema), func_(func), column_(column) {}

bool AggregateOperator::open(std::string& error) {
    count_ = 0;
    sum_ = 0;
    avgSum_ = 0;
    resultEmitted_ = false;
    
    std::size_t colIdx = schema_.columns.size();
    for (std::size_t i = 0; i < schema_.columns.size(); ++i) {
        if (schema_.columns[i].name == column_) {
            colIdx = i;
            break;
        }
    }

    if (!child_->open(error)) {
        return false;
    }

    Row row;
    while (child_->next(row, error)) {
        ++count_;
        
        if (colIdx < row.size() && !row[colIdx].isNull()) {
            if (row[colIdx].isInt()) {
                sum_ += row[colIdx].intVal;
                avgSum_ += static_cast<double>(row[colIdx].intVal);
            }
        }
    }
    
    child_->close();
    return true;
}

bool AggregateOperator::next(Row& outRow, std::string& error) {
    if (resultEmitted_) {
        return false;
    }

    outRow.clear();
    
    switch (func_) {
        case AggFunc::Count:
            outRow.push_back(Value::makeInt(count_));
            break;
        case AggFunc::Sum:
            outRow.push_back(Value::makeInt(sum_));
            break;
        case AggFunc::Avg:
            outRow.push_back(Value::makeInt(count_ > 0 ? static_cast<long long>(avgSum_ / count_) : 0));
            break;
    }
    
    resultEmitted_ = true;
    return true;
}

void AggregateOperator::close() {
    count_ = 0;
    sum_ = 0;
    avgSum_ = 0;
    resultEmitted_ = false;
}

// Week 15-16: SortOperator
SortOperator::SortOperator(std::unique_ptr<IOperator> child,
                           const TableSchema& schema,
                           std::vector<SortKey> sortKeys)
    : child_(std::move(child)), schema_(schema), sortKeys_(std::move(sortKeys)) {}

bool SortOperator::open(std::string& error) {
    sortedRows_.clear();
    index_ = 0;
    
    if (!child_->open(error)) {
        return false;
    }

    Row row;
    while (child_->next(row, error)) {
        sortedRows_.push_back(row);
    }
    child_->close();

    // Bubble sort (simple for now)
    if (!sortedRows_.empty() && !sortKeys_.empty()) {
        for (std::size_t i = 0; i < sortedRows_.size(); ++i) {
            for (std::size_t j = 0; j < sortedRows_.size() - 1 - i; ++j) {
                if (compareRows(sortedRows_[j + 1], sortedRows_[j], schema_)) {
                    std::swap(sortedRows_[j], sortedRows_[j + 1]);
                }
            }
        }
    }

    return true;
}

bool SortOperator::next(Row& outRow, std::string&) {
    if (index_ >= sortedRows_.size()) {
        return false;
    }

    outRow = sortedRows_[index_++];
    return true;
}

void SortOperator::close() {
    sortedRows_.clear();
    index_ = 0;
}

bool SortOperator::compareRows(const Row& left, const Row& right, const TableSchema& schema) const {
    for (const SortKey& key : sortKeys_) {
        std::size_t colIdx = schema.columns.size();
        for (std::size_t i = 0; i < schema.columns.size(); ++i) {
            if (schema.columns[i].name == key.column) {
                colIdx = i;
                break;
            }
        }

        if (colIdx < left.size() && colIdx < right.size()) {
            int cmp = compareValues(left[colIdx], right[colIdx]);
            if (cmp != 0) {
                // For ascending: swap if left < right (cmp < 0)
                // For descending: swap if left > right (cmp > 0)
                return (cmp < 0 && !key.descending) || (cmp > 0 && key.descending);
            }
        }
    }
    return false;
}

int SortOperator::compareValues(const Value& a, const Value& b) const {
    if (a.isNull() && b.isNull()) return 0;
    if (a.isNull()) return -1;
    if (b.isNull()) return 1;

    if (a.isInt() && b.isInt()) {
        if (a.intVal < b.intVal) return -1;
        if (a.intVal > b.intVal) return 1;
        return 0;
    }

    if (a.isString() && b.isString()) {
        return a.strVal.compare(b.strVal);
    }

    return 0;
}

// Week 15-16: GroupByOperator
GroupByOperator::GroupByOperator(std::unique_ptr<IOperator> child,
                                 const TableSchema& schema,
                                 std::vector<std::string> groupColumns)
    : child_(std::move(child)), schema_(schema), groupColumns_(std::move(groupColumns)) {}

bool GroupByOperator::open(std::string& error) {
    groupedRows_.clear();
    index_ = 0;

    if (!child_->open(error)) {
        return false;
    }

    Row row;
    while (child_->next(row, error)) {
        groupedRows_.push_back(row);
    }
    child_->close();

    return true;
}

bool GroupByOperator::next(Row& outRow, std::string& error) {
    if (index_ >= groupedRows_.size()) {
        return false;
    }

    outRow = groupedRows_[index_++];
    return true;
}

void GroupByOperator::close() {
    groupedRows_.clear();
    index_ = 0;
}

PlanNode ExecutionEngine::buildSelectPlan(const std::vector<Predicate>& predicates,
                                          const std::vector<std::string>& projectionColumns) const {
    PlanNode scan;
    scan.type = PlanNodeType::Scan;
    scan.detail = "TableScan";

    PlanNode root = scan;

    if (!predicates.empty()) {
        PlanNode filter;
        filter.type = PlanNodeType::Filter;
        filter.detail = "Filter(" + std::to_string(predicates.size()) + " predicates)";
        filter.children.push_back(root);
        root = std::move(filter);
    }

    if (!projectionColumns.empty()) {
        PlanNode project;
        project.type = PlanNodeType::Project;
        project.detail = "Project(" + std::to_string(projectionColumns.size()) + " columns)";
        project.children.push_back(root);
        root = std::move(project);
    }

    return root;
}

bool ExecutionEngine::executeSelect(TableHeap& heap,
                                    const TableSchema& schema,
                                    const std::vector<Predicate>& predicates,
                                    const std::vector<std::string>& projectionColumns,
                                    std::vector<Row>& outRows,
                                    std::vector<ColumnDefinition>& outColumns,
                                    std::string& error) const {
    outRows.clear();
    outColumns.clear();

    std::vector<std::size_t> projectionIndices;
    if (!projectionColumns.empty()) {
        std::unordered_map<std::string, std::size_t> indexMap;
        for (std::size_t i = 0; i < schema.columns.size(); ++i) {
            indexMap[schema.columns[i].name] = i;
        }

        for (const std::string& columnName : projectionColumns) {
            const auto it = indexMap.find(columnName);
            if (it == indexMap.end()) {
                error = "Unknown projection column: " + columnName;
                return false;
            }
            projectionIndices.push_back(it->second);
            outColumns.push_back(schema.columns[it->second]);
        }
    } else {
        outColumns = schema.columns;
    }

    std::unique_ptr<IOperator> root = std::make_unique<ScanOperator>(heap, schema);

    if (!predicates.empty()) {
        root = std::make_unique<FilterOperator>(std::move(root), schema, predicates);
    }

    if (!projectionColumns.empty()) {
        root = std::make_unique<ProjectOperator>(
            std::move(root), schema, projectionColumns, projectionIndices);
    }

    if (!root->open(error)) {
        return false;
    }

    Row row;
    while (root->next(row, error)) {
        outRows.push_back(row);
    }

    root->close();
    return true;
}

} // namespace advdb
