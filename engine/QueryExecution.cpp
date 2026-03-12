#include "QueryExecution.h"

#include <unordered_map>
#include <utility>

namespace advdb {

ScanOperator::ScanOperator(TableHeap& heap, const TableSchema& schema)
    : heap_(heap), schema_(schema) {}

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
