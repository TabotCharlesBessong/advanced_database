#pragma once

#include <cstddef>
#include <string>

namespace advdb {

enum class SqlTokenType {
    End,
    Identifier,
    Number,
    String,

    Comma,
    LParen,
    RParen,
    Semicolon,
    Star,

    Eq,
    Neq,
    Lt,
    Lte,
    Gt,
    Gte,

    // Keywords
    Create,
    Table,
    Insert,
    Into,
    Values,
    Select,
    From,
    Where,
    Int,
    Text,
    Varchar,
    Null,
    Not
};

struct SqlToken {
    SqlTokenType type{SqlTokenType::End};
    std::string lexeme;
    std::size_t line{1};
    std::size_t column{1};
};

struct SqlParseError {
    std::string message;
    std::size_t line{1};
    std::size_t column{1};
};

} // namespace advdb
