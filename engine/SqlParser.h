#pragma once

#include <vector>

#include "SqlAst.h"
#include "SqlToken.h"

namespace advdb {

class SqlParser {
public:
    explicit SqlParser(std::vector<SqlToken> tokens);

    bool parse(SqlStatement& outStatement, SqlParseError& error);

private:
    bool parseCreateTable(SqlStatement& outStatement, SqlParseError& error);
    bool parseInsert(SqlStatement& outStatement, SqlParseError& error);
    bool parseSelect(SqlStatement& outStatement, SqlParseError& error);

    bool parseJoinClause(SqlJoinClause& outJoin, SqlParseError& error) = delete;
    bool parseGroupByClause(SqlGroupByClause& outGroupBy, SqlParseError& error) = delete;
    bool parseHavingClause(SqlHavingClause& outHaving, SqlParseError& error) = delete;
    bool parseOrderByClause(SqlOrderByClause& outOrderBy, SqlParseError& error) = delete;

    bool parseColumnType(ColumnType& outType, std::uint32_t& outVarcharLength, SqlParseError& error);
    bool parseLiteral(SqlLiteral& outLiteral, SqlParseError& error);

    bool match(SqlTokenType type);
    bool check(SqlTokenType type) const;
    const SqlToken& advance();
    const SqlToken& peek() const;
    const SqlToken& previous() const;
    bool isAtEnd() const;

    bool consume(SqlTokenType type, const std::string& message, SqlParseError& error);
    SqlParseError makeError(const std::string& message) const;

    std::vector<SqlToken> tokens_;
    std::size_t current_{0};
};

} // namespace advdb
