#include "Database.h"

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

bool parseTypeSpec(const std::string& typeSpec, advdb::ColumnType& type, std::uint32_t& varcharLength, bool& nullable, std::string& error) {
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
        std::string lenStr = working.substr(prefix.size(), working.size() - prefix.size() - 1U);
        if (lenStr.empty()) {
            error = "varchar length is required";
            return false;
        }

        std::uint32_t length = static_cast<std::uint32_t>(std::stoul(lenStr));
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
        case advdb::ColumnType::Int:
            return "int";
        case advdb::ColumnType::Text:
            return "text";
        case advdb::ColumnType::Varchar:
            return "varchar(" + std::to_string(column.varcharLength) + ")";
    }
    return "unknown";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "{\"ok\":false,\"error\":\"Usage: dbcore_engine <command> <dbPath> [args]\"}" << std::endl;
        return 1;
    }

    const std::string command = argv[1];
    const std::string dbPath = argv[2];

    Database db(dbPath);
    if (!db.initialize()) {
        std::cout << "{\"ok\":false,\"error\":\"Database initialization failed\"}" << std::endl;
        return 1;
    }

    if (command == "init") {
        std::cout << "{\"ok\":true}" << std::endl;
        return 0;
    }

    if (command == "create_table") {
        if (argc < 5) {
            std::cout << "{\"ok\":false,\"error\":\"create_table requires table name and column spec\"}" << std::endl;
            return 1;
        }

        advdb::TableSchema schema;
        schema.name = argv[3];

        std::vector<std::string> columnSpecs = split(argv[4], ',');
        for (const std::string& columnSpec : columnSpecs) {
            const std::vector<std::string> parts = split(columnSpec, ':');
            if (parts.size() != 2U) {
                std::cout << "{\"ok\":false,\"error\":\"Invalid column spec\"}" << std::endl;
                return 1;
            }

            advdb::ColumnType type;
            std::uint32_t varcharLength = 0U;
            bool nullable = false;
            std::string parseError;
            if (!parseTypeSpec(parts[1], type, varcharLength, nullable, parseError)) {
                std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(parseError) << "\"}" << std::endl;
                return 1;
            }

            schema.columns.push_back(advdb::ColumnDefinition{parts[0], type, varcharLength, nullable});
        }

        std::string error;
        if (!db.createTable(schema, error)) {
            std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(error) << "\"}" << std::endl;
            return 1;
        }

        std::cout << "{\"ok\":true,\"table\":\"" << jsonEscape(schema.name) << "\"}" << std::endl;
        return 0;
    }

    if (command == "list_tables") {
        const std::vector<std::string> names = db.listTables();
        std::cout << "{\"ok\":true,\"tables\":[";
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (i > 0U) {
                std::cout << ',';
            }
            std::cout << '"' << jsonEscape(names[i]) << '"';
        }
        std::cout << "]}" << std::endl;
        return 0;
    }

    if (command == "describe_table") {
        if (argc < 4) {
            std::cout << "{\"ok\":false,\"error\":\"describe_table requires table name\"}" << std::endl;
            return 1;
        }

        advdb::TableSchema schema;
        std::string error;
        if (!db.getTable(argv[3], schema, error)) {
            std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(error) << "\"}" << std::endl;
            return 1;
        }

        std::cout << "{\"ok\":true,\"table\":{\"name\":\"" << jsonEscape(schema.name) << "\",\"columns\":[";
        for (std::size_t i = 0; i < schema.columns.size(); ++i) {
            if (i > 0U) {
                std::cout << ',';
            }
            const advdb::ColumnDefinition& column = schema.columns[i];
            std::cout << "{\"name\":\"" << jsonEscape(column.name)
                      << "\",\"type\":\"" << jsonEscape(typeToSpec(column))
                      << "\",\"nullable\":" << (column.nullable ? "true" : "false")
                      << "}";
        }
        std::cout << "]}}" << std::endl;
        return 0;
    }

    std::cout << "{\"ok\":false,\"error\":\"Unknown command\"}" << std::endl;
    return 1;
}
