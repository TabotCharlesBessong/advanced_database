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
};

using SqlStatement = std::variant<SqlCreateTableStatement, SqlInsertStatement, SqlSelectStatement>;

} // namespace advdb
