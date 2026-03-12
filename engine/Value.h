#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace advdb {

// A single typed value in a row (INT, VARCHAR/TEXT, or NULL).
struct Value {
    enum class Kind { Null, Int, String };

    Kind kind = Kind::Null;
    int64_t intVal = 0;
    std::string strVal;

    static Value makeNull() { return {}; }

    static Value makeInt(int64_t v) {
        Value val;
        val.kind = Kind::Int;
        val.intVal = v;
        return val;
    }

    static Value makeString(std::string s) {
        Value val;
        val.kind = Kind::String;
        val.strVal = std::move(s);
        return val;
    }

    bool isNull() const { return kind == Kind::Null; }
    bool isInt() const { return kind == Kind::Int; }
    bool isString() const { return kind == Kind::String; }
};

// A row is an ordered sequence of values matching a TableSchema's columns.
using Row = std::vector<Value>;

} // namespace advdb
