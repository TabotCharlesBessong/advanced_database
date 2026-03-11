#include "Database.h"
#include "Predicate.h"
#include "Value.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string jsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        if (c == '\\' || c == '"') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

std::vector<std::string> split(const std::string& input, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(input);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        parts.push_back(token);
    }
    return parts;
}

bool parseTypeSpec(const std::string& typeSpec,
                   advdb::ColumnType& type,
                   std::uint32_t& varcharLength,
                   bool& nullable,
                   std::string& error) {
    nullable = false;
    std::string working = typeSpec;
    if (!working.empty() && working.back() == '?') {
        nullable = true;
        working.pop_back();
    }

    if (working == "int") {
        type = advdb::ColumnType::Int;
        varcharLength = 0U;
        return true;
    }

    if (working == "text") {
        type = advdb::ColumnType::Text;
        varcharLength = 0U;
        return true;
    }

    const std::string prefix = "varchar(";
    if (working.rfind(prefix, 0) == 0 && working.back() == ')') {
        const std::string lenStr =
            working.substr(prefix.size(), working.size() - prefix.size() - 1U);
        if (lenStr.empty()) {
            error = "varchar length is required";
            return false;
        }
        const std::uint32_t length = static_cast<std::uint32_t>(std::stoul(lenStr));
        if (length == 0U) {
            error = "varchar length must be greater than zero";
            return false;
        }
        type = advdb::ColumnType::Varchar;
        varcharLength = length;
        return true;
    }

    error = "Unsupported type spec: " + typeSpec;
    return false;
}

std::string typeToSpec(const advdb::ColumnDefinition& column) {
    switch (column.type) {
        case advdb::ColumnType::Int:     return "int";
        case advdb::ColumnType::Text:    return "text";
        case advdb::ColumnType::Varchar:
            return "varchar(" + std::to_string(column.varcharLength) + ")";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Row spec parser for the `insert` command:
//   colName=value,...
//   INT columns: bare integer
//   VARCHAR/TEXT: bare string (quotes stripped if present)
//   NULL: literal null or NULL
// ---------------------------------------------------------------------------
bool parseRowSpec(const std::string& spec,
                  const advdb::TableSchema& schema,
                  advdb::Row& row,
                  std::string& error) {
    const std::vector<std::string> specs = split(spec, ',');
    if (specs.size() != schema.columns.size()) {
        error = "Expected " + std::to_string(schema.columns.size()) +
                " column values, got " + std::to_string(specs.size());
        return false;
    }

    row.resize(schema.columns.size());
    for (std::size_t i = 0u; i < specs.size(); ++i) {
        const std::size_t eq = specs[i].find('=');
        if (eq == std::string::npos) {
            error = "Missing '=' in column spec: " + specs[i];
            return false;
        }
        const std::string colName = specs[i].substr(0u, eq);
        const std::string rawVal  = specs[i].substr(eq + 1u);

        std::size_t colIdx = schema.columns.size();
        for (std::size_t j = 0u; j < schema.columns.size(); ++j) {
            if (schema.columns[j].name == colName) {
                colIdx = j;
                break;
            }
        }
        if (colIdx == schema.columns.size()) {
            error = "Unknown column: " + colName;
            return false;
        }

        if (rawVal == "null" || rawVal == "NULL") {
            row[colIdx] = advdb::Value::makeNull();
        } else if (schema.columns[colIdx].type == advdb::ColumnType::Int) {
            try {
                row[colIdx] = advdb::Value::makeInt(std::stoll(rawVal));
            } catch (...) {
                error = "Invalid integer value: " + rawVal;
                return false;
            }
        } else {
            std::string s = rawVal;
            if (s.size() >= 2u && s.front() == '\'' && s.back() == '\'') {
                s = s.substr(1u, s.size() - 2u);
            }
            row[colIdx] = advdb::Value::makeString(s);
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Predicate spec parser for the `select` command:
//   col:op:val[,col:op:val,...]   (AND semantics)
//   Supported ops: eq neq lt lte gt gte
// ---------------------------------------------------------------------------
bool parsePredicates(const std::string& spec,
                     const advdb::TableSchema& schema,
                     std::vector<advdb::Predicate>& preds,
                     std::string& error) {
    if (spec.empty()) {
        return true;
    }

    for (const std::string& part : split(spec, ',')) {
        const std::vector<std::string> tokens = split(part, ':');
        if (tokens.size() < 3u) {
            error = "Invalid predicate: " + part + " (expected col:op:val)";
            return false;
        }

        advdb::Predicate pred;
        pred.column = tokens[0];
        const std::string opStr = tokens[1];
        std::string valStr;
        for (std::size_t i = 2u; i < tokens.size(); ++i) {
            if (i > 2u) valStr += ':';
            valStr += tokens[i];
        }

        if      (opStr == "eq")  pred.op = advdb::CompareOp::EQ;
        else if (opStr == "neq") pred.op = advdb::CompareOp::NEQ;
        else if (opStr == "lt")  pred.op = advdb::CompareOp::LT;
        else if (opStr == "lte") pred.op = advdb::CompareOp::LTE;
        else if (opStr == "gt")  pred.op = advdb::CompareOp::GT;
        else if (opStr == "gte") pred.op = advdb::CompareOp::GTE;
        else {
            error = "Unknown operator: " + opStr;
            return false;
        }

        advdb::ColumnType colType = advdb::ColumnType::Text;
        for (const auto& col : schema.columns) {
            if (col.name == pred.column) {
                colType = col.type;
                break;
            }
        }

        if (colType == advdb::ColumnType::Int) {
            try {
                pred.value = advdb::Value::makeInt(std::stoll(valStr));
            } catch (...) {
                error = "Invalid integer in predicate: " + valStr;
                return false;
            }
        } else {
            std::string s = valStr;
            if (s.size() >= 2u && s.front() == '\'' && s.back() == '\'') {
                s = s.substr(1u, s.size() - 2u);
            }
            pred.value = advdb::Value::makeString(s);
        }

        preds.push_back(std::move(pred));
    }
    return true;
}

std::string valueToJson(const advdb::Value& val, const advdb::ColumnDefinition& col) {
    if (val.isNull()) {
        return "null";
    }
    if (col.type == advdb::ColumnType::Int) {
        return std::to_string(val.intVal);
    }
    return "\"" + jsonEscape(val.strVal) + "\"";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << R"({"ok":false,"error":"Usage: dbcore_engine <command> <dbPath> [args]"})"
                  << std::endl;
        return 1;
    }

    const std::string command = argv[1];
    const std::string dbPath  = argv[2];

    Database db(dbPath);
    if (!db.initialize()) {
        std::cout << R"({"ok":false,"error":"Database initialization failed"})" << std::endl;
        return 1;
    }

    // ---- init ---------------------------------------------------------------
    if (command == "init") {
        std::cout << R"({"ok":true})" << std::endl;
        return 0;
    }

    // ---- create_table -------------------------------------------------------
    if (command == "create_table") {
        if (argc < 5) {
            std::cout << R"({"ok":false,"error":"create_table requires table name and column spec"})"
                      << std::endl;
            return 1;
        }

        advdb::TableSchema schema;
        schema.name = argv[3];

        for (const std::string& columnSpec : split(argv[4], ',')) {
            const std::vector<std::string> parts = split(columnSpec, ':');
            if (parts.size() != 2U) {
                std::cout << R"({"ok":false,"error":"Invalid column spec"})" << std::endl;
                return 1;
            }
            advdb::ColumnType type;
            std::uint32_t varcharLength = 0U;
            bool nullable = false;
            std::string parseError;
            if (!parseTypeSpec(parts[1], type, varcharLength, nullable, parseError)) {
                std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(parseError) << "\"}"
                          << std::endl;
                return 1;
            }
            schema.columns.push_back(
                advdb::ColumnDefinition{parts[0], type, varcharLength, nullable});
        }

        std::string error;
        if (!db.createTable(schema, error)) {
            std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(error) << "\"}" << std::endl;
            return 1;
        }
        std::cout << "{\"ok\":true,\"table\":\"" << jsonEscape(schema.name) << "\"}" << std::endl;
        return 0;
    }

    // ---- list_tables --------------------------------------------------------
    if (command == "list_tables") {
        const std::vector<std::string> names = db.listTables();
        std::cout << "{\"ok\":true,\"tables\":[";
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (i > 0U) std::cout << ',';
            std::cout << '"' << jsonEscape(names[i]) << '"';
        }
        std::cout << "]}" << std::endl;
        return 0;
    }

    // ---- describe_table -----------------------------------------------------
    if (command == "describe_table") {
        if (argc < 4) {
            std::cout << R"({"ok":false,"error":"describe_table requires table name"})"
                      << std::endl;
            return 1;
        }
        advdb::TableSchema schema;
        std::string error;
        if (!db.getTable(argv[3], schema, error)) {
            std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(error) << "\"}" << std::endl;
            return 1;
        }
        std::cout << "{\"ok\":true,\"table\":{\"name\":\"" << jsonEscape(schema.name)
                  << "\",\"columns\":[";
        for (std::size_t i = 0; i < schema.columns.size(); ++i) {
            if (i > 0U) std::cout << ',';
            const advdb::ColumnDefinition& col = schema.columns[i];
            std::cout << "{\"name\":\"" << jsonEscape(col.name)
                      << "\",\"type\":\"" << jsonEscape(typeToSpec(col))
                      << "\",\"nullable\":" << (col.nullable ? "true" : "false")
                      << "}";
        }
        std::cout << "]}}" << std::endl;
        return 0;
    }

    // ---- insert -------------------------------------------------------------
    // Usage: dbcore_engine insert <dbPath> <tableName> <col=val,...>
    if (command == "insert") {
        if (argc < 5) {
            std::cout << R"({"ok":false,"error":"insert requires table name and col=val spec"})"
                      << std::endl;
            return 1;
        }
        const std::string tableName = argv[3];
        advdb::TableSchema schema;
        std::string error;
        if (!db.getTable(tableName, schema, error)) {
            std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(error) << "\"}" << std::endl;
            return 1;
        }

        advdb::Row row;
        if (!parseRowSpec(argv[4], schema, row, error)) {
            std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(error) << "\"}" << std::endl;
            return 1;
        }

        if (!db.insertRow(tableName, row, error)) {
            std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(error) << "\"}" << std::endl;
            return 1;
        }
        std::cout << "{\"ok\":true,\"table\":\"" << jsonEscape(tableName) << "\"}" << std::endl;
        return 0;
    }

    // ---- select -------------------------------------------------------------
    // Usage: dbcore_engine select <dbPath> <tableName> [<col:op:val,...>]
    if (command == "select") {
        if (argc < 4) {
            std::cout << R"({"ok":false,"error":"select requires table name"})" << std::endl;
            return 1;
        }
        const std::string tableName = argv[3];
        advdb::TableSchema schema;
        std::string error;
        if (!db.getTable(tableName, schema, error)) {
            std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(error) << "\"}" << std::endl;
            return 1;
        }

        std::vector<advdb::Predicate> preds;
        if (argc >= 5) {
            if (!parsePredicates(argv[4], schema, preds, error)) {
                std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(error) << "\"}"
                          << std::endl;
                return 1;
            }
        }

        std::vector<advdb::Row> rows;
        if (!db.selectRows(tableName, preds, rows, error)) {
            std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(error) << "\"}" << std::endl;
            return 1;
        }

        std::cout << "{\"ok\":true,\"rows\":[";
        for (std::size_t r = 0u; r < rows.size(); ++r) {
            if (r > 0u) std::cout << ',';
            std::cout << '{';
            for (std::size_t c = 0u; c < schema.columns.size(); ++c) {
                if (c > 0u) std::cout << ',';
                std::cout << '"' << jsonEscape(schema.columns[c].name) << "\":"
                          << valueToJson(rows[r][c], schema.columns[c]);
            }
            std::cout << '}';
        }
        std::cout << "]}" << std::endl;
        return 0;
    }

    std::cout << R"({"ok":false,"error":"Unknown command"})" << std::endl;
    return 1;
}
