#include "SqlLexer.h"

#include <cctype>
#include <unordered_map>

namespace advdb {

namespace {

std::string upper(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return out;
}

} // namespace

SqlLexer::SqlLexer(std::string input) : input_(std::move(input)) {}

bool SqlLexer::tokenize(std::vector<SqlToken>& outTokens, SqlParseError& error) {
    outTokens.clear();

    while (true) {
        skipWhitespace();

        SqlToken token;
        if (!scanToken(token, error)) {
            return false;
        }

        outTokens.push_back(token);
        if (token.type == SqlTokenType::End) {
            return true;
        }
    }
}

bool SqlLexer::scanToken(SqlToken& token, SqlParseError& error) {
    if (isAtEnd()) {
        token = makeToken(SqlTokenType::End, "", line_, col_);
        return true;
    }

    const std::size_t startLine = line_;
    const std::size_t startCol = col_;
    const char c = advance();

    switch (c) {
        case ',': token = makeToken(SqlTokenType::Comma, ",", startLine, startCol); return true;
        case '(': token = makeToken(SqlTokenType::LParen, "(", startLine, startCol); return true;
        case ')': token = makeToken(SqlTokenType::RParen, ")", startLine, startCol); return true;
        case ';': token = makeToken(SqlTokenType::Semicolon, ";", startLine, startCol); return true;
        case '*': token = makeToken(SqlTokenType::Star, "*", startLine, startCol); return true;
        case '=': token = makeToken(SqlTokenType::Eq, "=", startLine, startCol); return true;
        case '!':
            if (peek() == '=') {
                advance();
                token = makeToken(SqlTokenType::Neq, "!=", startLine, startCol);
                return true;
            }
            error = {"Unexpected character '!'. Did you mean '!='?", startLine, startCol};
            return false;
        case '<':
            if (peek() == '=') {
                advance();
                token = makeToken(SqlTokenType::Lte, "<=", startLine, startCol);
            } else {
                token = makeToken(SqlTokenType::Lt, "<", startLine, startCol);
            }
            return true;
        case '>':
            if (peek() == '=') {
                advance();
                token = makeToken(SqlTokenType::Gte, ">=", startLine, startCol);
            } else {
                token = makeToken(SqlTokenType::Gt, ">", startLine, startCol);
            }
            return true;
        case '\'': {
            std::string value;
            while (!isAtEnd() && peek() != '\'') {
                const char ch = advance();
                if (ch == '\\' && !isAtEnd()) {
                    value.push_back(advance());
                } else {
                    value.push_back(ch);
                }
            }

            if (isAtEnd()) {
                error = {"Unterminated string literal", startLine, startCol};
                return false;
            }

            advance();
            token = makeToken(SqlTokenType::String, value, startLine, startCol);
            return true;
        }
        default:
            break;
    }

    if (std::isdigit(static_cast<unsigned char>(c))) {
        std::string num(1, c);
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            num.push_back(advance());
        }
        token = makeToken(SqlTokenType::Number, num, startLine, startCol);
        return true;
    }

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        std::string ident(1, c);
        while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
            ident.push_back(advance());
        }

        static const std::unordered_map<std::string, SqlTokenType> keywords = {
            {"CREATE", SqlTokenType::Create},
            {"TABLE", SqlTokenType::Table},
            {"INSERT", SqlTokenType::Insert},
            {"INTO", SqlTokenType::Into},
            {"VALUES", SqlTokenType::Values},
            {"SELECT", SqlTokenType::Select},
            {"FROM", SqlTokenType::From},
            {"WHERE", SqlTokenType::Where},
            {"INT", SqlTokenType::Int},
            {"TEXT", SqlTokenType::Text},
            {"VARCHAR", SqlTokenType::Varchar},
            {"NULL", SqlTokenType::Null},
            {"NOT", SqlTokenType::Not}
        };

        const std::string up = upper(ident);
        const auto it = keywords.find(up);
        if (it != keywords.end()) {
            token = makeToken(it->second, ident, startLine, startCol);
        } else {
            token = makeToken(SqlTokenType::Identifier, ident, startLine, startCol);
        }
        return true;
    }

    error = {"Unexpected character '" + std::string(1, c) + "'", startLine, startCol};
    return false;
}

void SqlLexer::skipWhitespace() {
    while (!isAtEnd()) {
        const char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
            continue;
        }
        if (c == '\n') {
            advance();
            continue;
        }
        break;
    }
}

char SqlLexer::peek(std::size_t offset) const {
    const std::size_t idx = pos_ + offset;
    if (idx >= input_.size()) {
        return '\0';
    }
    return input_[idx];
}

bool SqlLexer::isAtEnd() const {
    return pos_ >= input_.size();
}

char SqlLexer::advance() {
    if (isAtEnd()) {
        return '\0';
    }

    const char c = input_[pos_++];
    if (c == '\n') {
        ++line_;
        col_ = 1;
    } else {
        ++col_;
    }
    return c;
}

SqlToken SqlLexer::makeToken(SqlTokenType type, const std::string& lexeme, std::size_t line, std::size_t col) const {
    SqlToken token;
    token.type = type;
    token.lexeme = lexeme;
    token.line = line;
    token.column = col;
    return token;
}

} // namespace advdb
