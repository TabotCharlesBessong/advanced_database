#pragma once

#include <string>
#include <variant>
#include <vector>

#include "CatalogTypes.h"

namespace advdb {

struct SqlLiteral {
    enum class Kind { Null, Int, String };

    Kind kind{Kind::Null};
    long long intValue{0};
    std::string stringValue;
};

struct SqlWhereClause {
    enum class Op { Eq, Neq, Lt, Lte, Gt, Gte };

    std::string column;
    Op op{Op::Eq};
    SqlLiteral literal;
};

struct SqlJoinClause {
    enum class Type { Inner, Left, Right };

    Type type{Type::Inner};
    std::string leftTable;    // driving/left-side table for this join
    std::string joinTable;
    std::string leftColumn;
    std::string rightColumn;  // from join table
};

struct SqlGroupByClause {
    std::vector<std::string> columns;
};

struct SqlHavingClause {
    std::string aggregateFunc;  // e.g., "COUNT", "SUM", "AVG"
    std::string aggregateColumn;  // column being aggregated
    SqlWhereClause::Op op{SqlWhereClause::Op::Eq};
    SqlLiteral value;
};

struct SqlOrderByClause {
    enum class Direction { Asc, Desc };

    std::vector<std::pair<std::string, Direction>> columns;
};

struct SqlCreateColumn {
    std::string name;
    ColumnType type{ColumnType::Int};
    std::uint32_t varcharLength{0};
    bool nullable{true};
};

struct SqlCreateTableStatement {
    std::string tableName;
    std::vector<SqlCreateColumn> columns;
};

struct SqlInsertStatement {
    std::string tableName;
    std::vector<SqlLiteral> values;
};

struct SqlSelectStatement {
    std::string tableName;
    std::vector<std::string> projection;
    bool selectAll{false};
    bool hasWhere{false};
    SqlWhereClause where;
    
    // Week 15-16: Advanced SQL Features
    std::vector<SqlJoinClause> joins;
    bool hasGroupBy{false};
    SqlGroupByClause groupBy;
    bool hasHaving{false};
    SqlHavingClause having;
    bool hasOrderBy{false};
    SqlOrderByClause orderBy;
};

using SqlStatement = std::variant<SqlCreateTableStatement, SqlInsertStatement, SqlSelectStatement>;

} // namespace advdb
