#pragma once

#include <string>
#include <vector>

#include "SqlToken.h"

namespace advdb {

class SqlLexer {
public:
    explicit SqlLexer(std::string input);

    bool tokenize(std::vector<SqlToken>& outTokens, SqlParseError& error);

private:
    bool scanToken(SqlToken& token, SqlParseError& error);
    void skipWhitespace();
    char peek(std::size_t offset = 0) const;
    bool isAtEnd() const;
    char advance();

    SqlToken makeToken(SqlTokenType type, const std::string& lexeme, std::size_t line, std::size_t col) const;

    std::string input_;
    std::size_t pos_{0};
    std::size_t line_{1};
    std::size_t col_{1};
};

} // namespace advdb
