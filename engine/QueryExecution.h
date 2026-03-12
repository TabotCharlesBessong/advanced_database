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
    IndexScan,
    Filter,
    Project,
    Join,
    GroupBy,
    Sort,
    Aggregate
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

// Week 19-20: Index scan operator (consumes pre-fetched index rows).
class IndexScanOperator final : public IOperator {
public:
    explicit IndexScanOperator(std::vector<Row> rows);

    bool open(std::string& error) override;
    bool next(Row& outRow, std::string& error) override;
    void close() override;

private:
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

// Week 15-16: Advanced SQL Features - Join Operator (nested loop join)
class JoinOperator final : public IOperator {
public:
    enum class JoinType { Inner, Left, Right };

    JoinOperator(std::unique_ptr<IOperator> leftChild,
                 std::unique_ptr<IOperator> rightChild,
                 const TableSchema& leftSchema,
                 const TableSchema& rightSchema,
                 const std::string& leftColumn,
                 const std::string& rightColumn,
                 JoinType joinType);

    bool open(std::string& error) override;
    bool next(Row& outRow, std::string& error) override;
    void close() override;

private:
    std::unique_ptr<IOperator> leftChild_;
    std::unique_ptr<IOperator> rightChild_;
    const TableSchema& leftSchema_;
    const TableSchema& rightSchema_;
    std::string leftColumn_;
    std::string rightColumn_;
    JoinType joinType_;
    
    std::vector<Row> leftRows_;
    std::vector<Row> rightRows_;
    std::size_t leftIndex_{0};
    std::size_t rightIndex_{0};
    bool rightRowsLoaded_{false};
};

// Week 15-16: Aggregate Operator - handles COUNT, SUM, AVG
class AggregateOperator final : public IOperator {
public:
    enum class AggFunc { Count, Sum, Avg };

    AggregateOperator(std::unique_ptr<IOperator> child,
                      const TableSchema& schema,
                      AggFunc func,
                      const std::string& column);

    bool open(std::string& error) override;
    bool next(Row& outRow, std::string& error) override;
    void close() override;

private:
    std::unique_ptr<IOperator> child_;
    const TableSchema& schema_;
    AggFunc func_;
    std::string column_;
    
    long long count_{0};
    long long sum_{0};
    double avgSum_{0};
    bool resultEmitted_{false};
};

// Week 15-16: Sort Operator - ORDER BY
class SortOperator final : public IOperator {
public:
    struct SortKey {
        std::string column;
        bool descending{false};
    };

    SortOperator(std::unique_ptr<IOperator> child,
                 const TableSchema& schema,
                 std::vector<SortKey> sortKeys);

    bool open(std::string& error) override;
    bool next(Row& outRow, std::string& error) override;
    void close() override;

private:
    std::unique_ptr<IOperator> child_;
    const TableSchema& schema_;
    std::vector<SortKey> sortKeys_;
    
    std::vector<Row> sortedRows_;
    std::size_t index_{0};
    
    bool compareRows(const Row& left, const Row& right, const TableSchema& schema) const;
    int compareValues(const Value& a, const Value& b) const;
};

// Week 15-16: GroupBy Operator
class GroupByOperator final : public IOperator {
public:
    GroupByOperator(std::unique_ptr<IOperator> child,
                    const TableSchema& schema,
                    std::vector<std::string> groupColumns);

    bool open(std::string& error) override;
    bool next(Row& outRow, std::string& error) override;
    void close() override;

private:
    std::unique_ptr<IOperator> child_;
    const TableSchema& schema_;
    std::vector<std::string> groupColumns_;
    
    std::vector<Row> groupedRows_;
    std::size_t index_{0};
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
