#include "SqlParser.h"

#include <cstdlib>

namespace advdb {

SqlParser::SqlParser(std::vector<SqlToken> tokens) : tokens_(std::move(tokens)) {}

bool SqlParser::parse(SqlStatement& outStatement, SqlParseError& error) {
    if (match(SqlTokenType::Create)) {
        if (!parseCreateTable(outStatement, error)) {
            return false;
        }
    } else if (match(SqlTokenType::Insert)) {
        if (!parseInsert(outStatement, error)) {
            return false;
        }
    } else if (match(SqlTokenType::Select)) {
        if (!parseSelect(outStatement, error)) {
            return false;
        }
    } else {
        error = makeError("Expected CREATE, INSERT, or SELECT");
        return false;
    }

    match(SqlTokenType::Semicolon);
    if (!isAtEnd()) {
        error = makeError("Unexpected tokens after end of statement");
        return false;
    }

    return true;
}

bool SqlParser::parseCreateTable(SqlStatement& outStatement, SqlParseError& error) {
    if (!consume(SqlTokenType::Table, "Expected TABLE after CREATE", error)) {
        return false;
    }

    if (!consume(SqlTokenType::Identifier, "Expected table name", error)) {
        return false;
    }
    SqlCreateTableStatement stmt;
    stmt.tableName = previous().lexeme;

    if (!consume(SqlTokenType::LParen, "Expected '(' after table name", error)) {
        return false;
    }

    do {
        if (!consume(SqlTokenType::Identifier, "Expected column name", error)) {
            return false;
        }

        SqlCreateColumn col;
        col.name = previous().lexeme;

        if (!parseColumnType(col.type, col.varcharLength, error)) {
            return false;
        }

        // Default nullable true unless NOT NULL present.
        col.nullable = true;
        if (match(SqlTokenType::Not)) {
            if (!consume(SqlTokenType::Null, "Expected NULL after NOT", error)) {
                return false;
            }
            col.nullable = false;
        } else if (match(SqlTokenType::Null)) {
            col.nullable = true;
        }

        stmt.columns.push_back(col);
    } while (match(SqlTokenType::Comma));

    if (!consume(SqlTokenType::RParen, "Expected ')' after column definitions", error)) {
        return false;
    }

    outStatement = stmt;
    return true;
}

bool SqlParser::parseInsert(SqlStatement& outStatement, SqlParseError& error) {
    if (!consume(SqlTokenType::Into, "Expected INTO after INSERT", error)) {
        return false;
    }
    if (!consume(SqlTokenType::Identifier, "Expected table name after INTO", error)) {
        return false;
    }

    SqlInsertStatement stmt;
    stmt.tableName = previous().lexeme;

    if (!consume(SqlTokenType::Values, "Expected VALUES", error)) {
        return false;
    }
    if (!consume(SqlTokenType::LParen, "Expected '(' after VALUES", error)) {
        return false;
    }

    do {
        SqlLiteral lit;
        if (!parseLiteral(lit, error)) {
            return false;
        }
        stmt.values.push_back(lit);
    } while (match(SqlTokenType::Comma));

    if (!consume(SqlTokenType::RParen, "Expected ')' after VALUES list", error)) {
        return false;
    }

    outStatement = stmt;
    return true;
}

bool SqlParser::parseSelect(SqlStatement& outStatement, SqlParseError& error) {
    SqlSelectStatement stmt;

    if (match(SqlTokenType::Star)) {
        stmt.selectAll = true;
    } else {
        do {
            if (!consume(SqlTokenType::Identifier, "Expected column name in SELECT list", error)) {
                return false;
            }
            stmt.projection.push_back(previous().lexeme);
        } while (match(SqlTokenType::Comma));
    }

    if (!consume(SqlTokenType::From, "Expected FROM in SELECT", error)) {
        return false;
    }
    if (!consume(SqlTokenType::Identifier, "Expected table name after FROM", error)) {
        return false;
    }
    stmt.tableName = previous().lexeme;

    if (match(SqlTokenType::Where)) {
        stmt.hasWhere = true;

        if (!consume(SqlTokenType::Identifier, "Expected column name after WHERE", error)) {
            return false;
        }
        stmt.where.column = previous().lexeme;

        if (match(SqlTokenType::Eq)) {
            stmt.where.op = SqlWhereClause::Op::Eq;
        } else if (match(SqlTokenType::Neq)) {
            stmt.where.op = SqlWhereClause::Op::Neq;
        } else if (match(SqlTokenType::Lt)) {
            stmt.where.op = SqlWhereClause::Op::Lt;
        } else if (match(SqlTokenType::Lte)) {
            stmt.where.op = SqlWhereClause::Op::Lte;
        } else if (match(SqlTokenType::Gt)) {
            stmt.where.op = SqlWhereClause::Op::Gt;
        } else if (match(SqlTokenType::Gte)) {
            stmt.where.op = SqlWhereClause::Op::Gte;
        } else {
            error = makeError("Expected comparison operator in WHERE clause");
            return false;
        }

        if (!parseLiteral(stmt.where.literal, error)) {
            return false;
        }
    }

    outStatement = stmt;
    return true;
}

bool SqlParser::parseColumnType(ColumnType& outType, std::uint32_t& outVarcharLength, SqlParseError& error) {
    outVarcharLength = 0;

    if (match(SqlTokenType::Int)) {
        outType = ColumnType::Int;
        return true;
    }
    if (match(SqlTokenType::Text)) {
        outType = ColumnType::Text;
        return true;
    }
    if (match(SqlTokenType::Varchar)) {
        outType = ColumnType::Varchar;
        if (!consume(SqlTokenType::LParen, "Expected '(' after VARCHAR", error)) {
            return false;
        }
        if (!consume(SqlTokenType::Number, "Expected VARCHAR length", error)) {
            return false;
        }
        outVarcharLength = static_cast<std::uint32_t>(std::strtoul(previous().lexeme.c_str(), nullptr, 10));
        if (outVarcharLength == 0) {
            error = makeError("VARCHAR length must be greater than zero");
            return false;
        }
        if (!consume(SqlTokenType::RParen, "Expected ')' after VARCHAR length", error)) {
            return false;
        }
        return true;
    }

    error = makeError("Expected column type (INT, TEXT, VARCHAR)");
    return false;
}

bool SqlParser::parseLiteral(SqlLiteral& outLiteral, SqlParseError& error) {
    if (match(SqlTokenType::Null)) {
        outLiteral.kind = SqlLiteral::Kind::Null;
        return true;
    }
    if (match(SqlTokenType::Number)) {
        outLiteral.kind = SqlLiteral::Kind::Int;
        outLiteral.intValue = std::strtoll(previous().lexeme.c_str(), nullptr, 10);
        return true;
    }
    if (match(SqlTokenType::String)) {
        outLiteral.kind = SqlLiteral::Kind::String;
        outLiteral.stringValue = previous().lexeme;
        return true;
    }

    error = makeError("Expected literal value (number, string, or NULL)");
    return false;
}

bool SqlParser::match(SqlTokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool SqlParser::check(SqlTokenType type) const {
    if (isAtEnd()) {
        return type == SqlTokenType::End;
    }
    return peek().type == type;
}

const SqlToken& SqlParser::advance() {
    if (!isAtEnd()) {
        ++current_;
    }
    return previous();
}

const SqlToken& SqlParser::peek() const {
    return tokens_[current_];
}

const SqlToken& SqlParser::previous() const {
    return tokens_[current_ - 1];
}

bool SqlParser::isAtEnd() const {
    return current_ >= tokens_.size() || tokens_[current_].type == SqlTokenType::End;
}

bool SqlParser::consume(SqlTokenType type, const std::string& message, SqlParseError& error) {
    if (check(type)) {
        advance();
        return true;
    }
    error = makeError(message);
    return false;
}

SqlParseError SqlParser::makeError(const std::string& message) const {
    const SqlToken& tok = peek();
    return SqlParseError{message, tok.line, tok.column};
}

} // namespace advdb
