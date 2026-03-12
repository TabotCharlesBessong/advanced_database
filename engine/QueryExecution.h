#pragma once

#include <memory>
#include <string>
#include <vector>

#include "CatalogTypes.h"
#include "Predicate.h"
#include "TableHeap.h"
#include "Value.h"

namespace advdb {

enum class PlanNodeType {
    Scan,
    Filter,
    Project
};

struct PlanNode {
    PlanNodeType type{PlanNodeType::Scan};
    std::string detail;
    std::vector<PlanNode> children;
};

class IOperator {
public:
    virtual ~IOperator() = default;

    virtual bool open(std::string& error) = 0;
    virtual bool next(Row& outRow, std::string& error) = 0;
    virtual void close() = 0;
};

class ScanOperator final : public IOperator {
public:
    ScanOperator(TableHeap& heap, const TableSchema& schema);

    bool open(std::string& error) override;
    bool next(Row& outRow, std::string& error) override;
    void close() override;

private:
    TableHeap& heap_;
    const TableSchema& schema_;
    std::vector<Row> rows_;
    std::size_t index_{0};
};

class FilterOperator final : public IOperator {
public:
    FilterOperator(std::unique_ptr<IOperator> child, const TableSchema& schema, std::vector<Predicate> predicates);

    bool open(std::string& error) override;
    bool next(Row& outRow, std::string& error) override;
    void close() override;

private:
    std::unique_ptr<IOperator> child_;
    const TableSchema& schema_;
    std::vector<Predicate> predicates_;
};

class ProjectOperator final : public IOperator {
public:
    ProjectOperator(std::unique_ptr<IOperator> child,
                    const TableSchema& inputSchema,
                    std::vector<std::string> projectionColumns,
                    std::vector<std::size_t> projectionIndices);

    bool open(std::string& error) override;
    bool next(Row& outRow, std::string& error) override;
    void close() override;

private:
    std::unique_ptr<IOperator> child_;
    const TableSchema& inputSchema_;
    std::vector<std::string> projectionColumns_;
    std::vector<std::size_t> projectionIndices_;
};

class ExecutionEngine {
public:
    PlanNode buildSelectPlan(const std::vector<Predicate>& predicates,
                             const std::vector<std::string>& projectionColumns) const;

    bool executeSelect(TableHeap& heap,
                       const TableSchema& schema,
                       const std::vector<Predicate>& predicates,
                       const std::vector<std::string>& projectionColumns,
                       std::vector<Row>& outRows,
                       std::vector<ColumnDefinition>& outColumns,
                       std::string& error) const;
};

} // namespace advdb
