#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

#include "BufferPool.h"
#include "Database.h"
#include "FileManager.h"
#include "Page.h"

#include "Predicate.h"
#include "RecordLayout.h"
#include "SqlAst.h"
#include "SqlLexer.h"
#include "SqlParser.h"
#include "SlottedPage.h"
#include "TableHeap.h"
#include "Value.h"
#include "Statistics.h"
#include "QueryPlanner.h"

namespace {

std::string tempPath(const std::string& baseName) {
    return baseName + "_tmp.db";
}

void cleanupFile(const std::string& path) {
    std::remove(path.c_str());
    std::remove((path + ".catalog").c_str());
}

} // namespace

TEST(DatabaseTest, Initialization) {
    const std::string dbPath = tempPath("init");
    cleanupFile(dbPath);

    Database db(dbPath);
    EXPECT_TRUE(db.initialize());
    EXPECT_EQ(db.getVersion(), "0.3.0");

    cleanupFile(dbPath);
}

TEST(DatabaseTest, CreateTableAndPersistCatalog) {
    const std::string dbPath = tempPath("catalog_persist");
    cleanupFile(dbPath);

    Database db(dbPath);
    ASSERT_TRUE(db.initialize());

    advdb::TableSchema schema;
    schema.name = "users";
    schema.columns = {
        advdb::ColumnDefinition{"id", advdb::ColumnType::Int, 0U, false},
        advdb::ColumnDefinition{"name", advdb::ColumnType::Varchar, 64U, false},
        advdb::ColumnDefinition{"bio", advdb::ColumnType::Text, 0U, true}
    };

    std::string error;
    ASSERT_TRUE(db.createTable(schema, error));

    const std::vector<std::string> tables = db.listTables();
    ASSERT_EQ(tables.size(), 1U);
    EXPECT_EQ(tables[0], "users");

    Database reopened(dbPath);
    ASSERT_TRUE(reopened.initialize());

    advdb::TableSchema loaded;
    ASSERT_TRUE(reopened.getTable("users", loaded, error));
    EXPECT_EQ(loaded.columns.size(), 3U);
    EXPECT_EQ(loaded.columns[1].name, "name");
    EXPECT_EQ(loaded.columns[1].varcharLength, 64U);

    cleanupFile(dbPath);
}

TEST(DatabaseTest, RejectInvalidSchemas) {
    const std::string dbPath = tempPath("invalid_schema");
    cleanupFile(dbPath);

    Database db(dbPath);
    ASSERT_TRUE(db.initialize());

    std::string error;
    advdb::TableSchema invalidName;
    invalidName.name = "1invalid";
    invalidName.columns = {advdb::ColumnDefinition{"id", advdb::ColumnType::Int, 0U, false}};
    EXPECT_FALSE(db.createTable(invalidName, error));

    advdb::TableSchema invalidVarchar;
    invalidVarchar.name = "people";
    invalidVarchar.columns = {advdb::ColumnDefinition{"name", advdb::ColumnType::Varchar, 0U, false}};
    EXPECT_FALSE(db.createTable(invalidVarchar, error));

    advdb::TableSchema duplicateColumns;
    duplicateColumns.name = "accounts";
    duplicateColumns.columns = {
        advdb::ColumnDefinition{"id", advdb::ColumnType::Int, 0U, false},
        advdb::ColumnDefinition{"id", advdb::ColumnType::Text, 0U, true}
    };
    EXPECT_FALSE(db.createTable(duplicateColumns, error));

    cleanupFile(dbPath);
}

TEST(StorageTest, PageSerializeDeserializeWithChecksum) {
    advdb::Page sourcePage;
    sourcePage.setPageId(7);
    sourcePage.setFlags(3);

    std::vector<std::uint8_t> payload(sourcePage.payloadSize(), 11U);
    ASSERT_TRUE(sourcePage.setPayload(payload));

    const std::vector<std::uint8_t> bytes = sourcePage.serialize();
    advdb::Page destinationPage;
    ASSERT_TRUE(destinationPage.deserialize(bytes));
    EXPECT_EQ(destinationPage.pageId(), 7U);
    EXPECT_EQ(destinationPage.flags(), 3U);
    EXPECT_EQ(destinationPage.payload()[0], 11U);

    std::vector<std::uint8_t> corrupted = bytes;
    corrupted.back() ^= 0xFF;
    advdb::Page corruptedPage;
    EXPECT_FALSE(corruptedPage.deserialize(corrupted));
}

TEST(StorageTest, FileManagerWriteReadRoundTrip) {
    const std::string filePath = tempPath("file_manager_roundtrip");
    cleanupFile(filePath);

    advdb::FileManager manager(filePath);
    ASSERT_TRUE(manager.open());

    advdb::Page page;
    page.setPageId(0);
    std::vector<std::uint8_t> payload(page.payloadSize(), 42U);
    ASSERT_TRUE(page.setPayload(payload));
    ASSERT_TRUE(manager.writePage(0, page.serialize(), advdb::StorageConfig::kDefaultPageSize));
    ASSERT_TRUE(manager.flush());
    manager.close();

    ASSERT_TRUE(manager.open(false));
    std::vector<std::uint8_t> buffer(advdb::StorageConfig::kDefaultPageSize, 0U);
    ASSERT_TRUE(manager.readPage(0, buffer, advdb::StorageConfig::kDefaultPageSize));

    advdb::Page loaded;
    ASSERT_TRUE(loaded.deserialize(buffer));
    EXPECT_EQ(loaded.payload()[0], 42U);

    manager.close();
    cleanupFile(filePath);
}

TEST(StorageTest, BufferPoolRespectsPinAndUsesLRU) {
    const std::string filePath = tempPath("buffer_pool_lru");
    cleanupFile(filePath);

    advdb::FileManager manager(filePath);
    ASSERT_TRUE(manager.open());

    advdb::BufferPool pool(2, manager);
    advdb::Page* mutablePage = nullptr;
    const advdb::Page* readonlyPage = nullptr;

    ASSERT_TRUE(pool.pinPageMutable(1, &mutablePage));
    std::vector<std::uint8_t> payload1(mutablePage->payloadSize(), 1U);
    ASSERT_TRUE(mutablePage->setPayload(payload1));
    ASSERT_TRUE(pool.unpinPage(1, true));

    ASSERT_TRUE(pool.pinPageMutable(2, &mutablePage));
    std::vector<std::uint8_t> payload2(mutablePage->payloadSize(), 2U);
    ASSERT_TRUE(mutablePage->setPayload(payload2));
    ASSERT_TRUE(pool.unpinPage(2, true));

    ASSERT_TRUE(pool.pinPage(1, &readonlyPage));
    EXPECT_EQ(readonlyPage->payload()[0], 1U);
    ASSERT_TRUE(pool.unpinPage(1, false));

    ASSERT_TRUE(pool.pinPageMutable(3, &mutablePage));
    std::vector<std::uint8_t> payload3(mutablePage->payloadSize(), 3U);
    ASSERT_TRUE(mutablePage->setPayload(payload3));
    ASSERT_TRUE(pool.unpinPage(3, true));

    ASSERT_TRUE(pool.flushAll());

    std::vector<std::uint8_t> raw(advdb::StorageConfig::kDefaultPageSize, 0U);
    ASSERT_TRUE(manager.readPage(1, raw, advdb::StorageConfig::kDefaultPageSize));
    advdb::Page page1;
    ASSERT_TRUE(page1.deserialize(raw));
    EXPECT_EQ(page1.payload()[0], 1U);

    ASSERT_TRUE(manager.readPage(3, raw, advdb::StorageConfig::kDefaultPageSize));
    advdb::Page page3;
    ASSERT_TRUE(page3.deserialize(raw));
    EXPECT_EQ(page3.payload()[0], 3U);

    manager.close();
    cleanupFile(filePath);
}

// ---------------------------------------------------------------------------
// Week 9-10 - CRUD tests
// ---------------------------------------------------------------------------

namespace {

advdb::TableSchema makeUsersSchema() {
    advdb::TableSchema schema;
    schema.name = "users";
    schema.columns = {
        advdb::ColumnDefinition{"id",   advdb::ColumnType::Int,     0U,  false},
        advdb::ColumnDefinition{"name", advdb::ColumnType::Varchar, 64U, false},
        advdb::ColumnDefinition{"bio",  advdb::ColumnType::Text,    0U,  true}
    };
    return schema;
}

} // namespace

TEST(RecordLayoutTest, EncodeDecodeRoundTrip) {
    const advdb::TableSchema schema = makeUsersSchema();

    advdb::Row row = {
        advdb::Value::makeInt(42),
        advdb::Value::makeString("Alice"),
        advdb::Value::makeString("Hello world")
    };

    const std::vector<uint8_t> encoded = advdb::RecordLayout::encode(schema, row);
    ASSERT_FALSE(encoded.empty());

    advdb::Row decoded;
    ASSERT_TRUE(advdb::RecordLayout::decode(schema, encoded, decoded));
    ASSERT_EQ(decoded.size(), 3U);
    EXPECT_EQ(decoded[0].intVal, 42);
    EXPECT_EQ(decoded[1].strVal, "Alice");
    EXPECT_EQ(decoded[2].strVal, "Hello world");
}

TEST(RecordLayoutTest, NullableRoundTrip) {
    const advdb::TableSchema schema = makeUsersSchema();

    advdb::Row row = {
        advdb::Value::makeInt(7),
        advdb::Value::makeString("Bob"),
        advdb::Value::makeNull()
    };

    const std::vector<uint8_t> encoded = advdb::RecordLayout::encode(schema, row);
    ASSERT_FALSE(encoded.empty());

    advdb::Row decoded;
    ASSERT_TRUE(advdb::RecordLayout::decode(schema, encoded, decoded));
    EXPECT_TRUE(decoded[2].isNull());
}

TEST(SlottedPageTest, InsertAndRetrieve) {
    advdb::Page page;
    page.setPageId(0U);

    advdb::SlottedPage sp(page);
    sp.init();

    const std::vector<uint8_t> data1 = {1, 2, 3};
    const std::vector<uint8_t> data2 = {10, 20};

    uint16_t slot1 = 99U;
    uint16_t slot2 = 99U;
    ASSERT_TRUE(sp.insertRecord(data1, slot1));
    ASSERT_TRUE(sp.insertRecord(data2, slot2));
    EXPECT_EQ(slot1, 0U);
    EXPECT_EQ(slot2, 1U);
    EXPECT_EQ(sp.numSlots(), 2U);

    std::vector<uint8_t> out;
    ASSERT_TRUE(sp.getRecord(0U, out));
    EXPECT_EQ(out, data1);
    ASSERT_TRUE(sp.getRecord(1U, out));
    EXPECT_EQ(out, data2);

    sp.commit();

    advdb::SlottedPage sp2(page);
    ASSERT_TRUE(sp2.load());
    EXPECT_EQ(sp2.numSlots(), 2U);
    ASSERT_TRUE(sp2.getRecord(0U, out));
    EXPECT_EQ(out, data1);
}

TEST(CRUDTest, InsertAndSelectAll) {
    const std::string dbPath = tempPath("crud_insert_select");
    cleanupFile(dbPath);
    std::remove((dbPath + ".users.heap").c_str());

    Database db(dbPath);
    ASSERT_TRUE(db.initialize());

    const advdb::TableSchema schema = makeUsersSchema();
    std::string error;
    ASSERT_TRUE(db.createTable(schema, error));

    const advdb::Row row1 = {
        advdb::Value::makeInt(1),
        advdb::Value::makeString("Alice"),
        advdb::Value::makeNull()
    };
    const advdb::Row row2 = {
        advdb::Value::makeInt(2),
        advdb::Value::makeString("Bob"),
        advdb::Value::makeString("backend dev")
    };

    ASSERT_TRUE(db.insertRow("users", row1, error)) << error;
    ASSERT_TRUE(db.insertRow("users", row2, error)) << error;

    std::vector<advdb::Row> rows;
    ASSERT_TRUE(db.selectRows("users", {}, rows, error)) << error;
    ASSERT_EQ(rows.size(), 2U);

    EXPECT_EQ(rows[0][0].intVal, 1);
    EXPECT_EQ(rows[0][1].strVal, "Alice");
    EXPECT_TRUE(rows[0][2].isNull());

    EXPECT_EQ(rows[1][0].intVal, 2);
    EXPECT_EQ(rows[1][1].strVal, "Bob");
    EXPECT_EQ(rows[1][2].strVal, "backend dev");

    cleanupFile(dbPath);
    std::remove((dbPath + ".users.heap").c_str());
}

TEST(CRUDTest, SelectWithWherePredicate) {
    const std::string dbPath = tempPath("crud_where");
    cleanupFile(dbPath);
    std::remove((dbPath + ".users.heap").c_str());

    Database db(dbPath);
    ASSERT_TRUE(db.initialize());

    const advdb::TableSchema schema = makeUsersSchema();
    std::string error;
    ASSERT_TRUE(db.createTable(schema, error));

    for (int i = 1; i <= 5; ++i) {
        const advdb::Row row = {
            advdb::Value::makeInt(i),
            advdb::Value::makeString("User" + std::to_string(i)),
            advdb::Value::makeNull()
        };
        ASSERT_TRUE(db.insertRow("users", row, error)) << error;
    }

    advdb::Predicate pred;
    pred.column = "id";
    pred.op     = advdb::CompareOp::GT;
    pred.value  = advdb::Value::makeInt(3);

    std::vector<advdb::Row> rows;
    ASSERT_TRUE(db.selectRows("users", {pred}, rows, error)) << error;
    ASSERT_EQ(rows.size(), 2U);
    EXPECT_EQ(rows[0][0].intVal, 4);
    EXPECT_EQ(rows[1][0].intVal, 5);

    cleanupFile(dbPath);
    std::remove((dbPath + ".users.heap").c_str());
}

TEST(CRUDTest, NullabilityEnforced) {
    const std::string dbPath = tempPath("crud_null");
    cleanupFile(dbPath);

    Database db(dbPath);
    ASSERT_TRUE(db.initialize());

    const advdb::TableSchema schema = makeUsersSchema();
    std::string error;
    ASSERT_TRUE(db.createTable(schema, error));

    const advdb::Row badRow = {
        advdb::Value::makeInt(1),
        advdb::Value::makeNull(),
        advdb::Value::makeNull()
    };
    EXPECT_FALSE(db.insertRow("users", badRow, error));

    cleanupFile(dbPath);
}

TEST(SqlLexerTest, TokenizesCreateInsertSelect) {
    const std::string sql = "CREATE TABLE users (id INT NOT NULL, name VARCHAR(64), bio TEXT NULL);";

    advdb::SqlLexer lexer(sql);
    std::vector<advdb::SqlToken> tokens;
    advdb::SqlParseError error;
    ASSERT_TRUE(lexer.tokenize(tokens, error)) << error.message;

    ASSERT_GE(tokens.size(), 10U);
    EXPECT_EQ(tokens[0].type, advdb::SqlTokenType::Create);
    EXPECT_EQ(tokens[1].type, advdb::SqlTokenType::Table);
    EXPECT_EQ(tokens[2].type, advdb::SqlTokenType::Identifier);
}

TEST(SqlLexerTest, ReportsUnterminatedString) {
    advdb::SqlLexer lexer("INSERT INTO users VALUES ('abc);");
    std::vector<advdb::SqlToken> tokens;
    advdb::SqlParseError error;
    EXPECT_FALSE(lexer.tokenize(tokens, error));
    EXPECT_FALSE(error.message.empty());
}

TEST(SqlParserTest, ParsesCreateTableToAst) {
    const std::string sql = "CREATE TABLE users (id INT NOT NULL, name VARCHAR(64), bio TEXT NULL);";

    advdb::SqlLexer lexer(sql);
    std::vector<advdb::SqlToken> tokens;
    advdb::SqlParseError error;
    ASSERT_TRUE(lexer.tokenize(tokens, error));

    advdb::SqlParser parser(tokens);
    advdb::SqlStatement stmt;
    ASSERT_TRUE(parser.parse(stmt, error)) << error.message;

    ASSERT_TRUE(std::holds_alternative<advdb::SqlCreateTableStatement>(stmt));
    const auto& create = std::get<advdb::SqlCreateTableStatement>(stmt);
    EXPECT_EQ(create.tableName, "users");
    ASSERT_EQ(create.columns.size(), 3U);
    EXPECT_EQ(create.columns[0].name, "id");
    EXPECT_EQ(create.columns[0].nullable, false);
    EXPECT_EQ(create.columns[1].type, advdb::ColumnType::Varchar);
    EXPECT_EQ(create.columns[1].varcharLength, 64U);
}

TEST(SqlParserTest, ParsesInsertAndSelectToAst) {
    {
        const std::string insertSql = "INSERT INTO users VALUES (1, 'Alice', NULL);";
        advdb::SqlLexer lexer(insertSql);
        std::vector<advdb::SqlToken> tokens;
        advdb::SqlParseError error;
        ASSERT_TRUE(lexer.tokenize(tokens, error));

        advdb::SqlParser parser(tokens);
        advdb::SqlStatement stmt;
        ASSERT_TRUE(parser.parse(stmt, error)) << error.message;
        ASSERT_TRUE(std::holds_alternative<advdb::SqlInsertStatement>(stmt));
        const auto& insert = std::get<advdb::SqlInsertStatement>(stmt);
        EXPECT_EQ(insert.tableName, "users");
        ASSERT_EQ(insert.values.size(), 3U);
        EXPECT_EQ(insert.values[0].kind, advdb::SqlLiteral::Kind::Int);
        EXPECT_EQ(insert.values[1].kind, advdb::SqlLiteral::Kind::String);
        EXPECT_EQ(insert.values[2].kind, advdb::SqlLiteral::Kind::Null);
    }

    {
        const std::string selectSql = "SELECT id, name FROM users WHERE id >= 10;";
        advdb::SqlLexer lexer(selectSql);
        std::vector<advdb::SqlToken> tokens;
        advdb::SqlParseError error;
        ASSERT_TRUE(lexer.tokenize(tokens, error));

        advdb::SqlParser parser(tokens);
        advdb::SqlStatement stmt;
        ASSERT_TRUE(parser.parse(stmt, error)) << error.message;
        ASSERT_TRUE(std::holds_alternative<advdb::SqlSelectStatement>(stmt));
        const auto& select = std::get<advdb::SqlSelectStatement>(stmt);
        EXPECT_EQ(select.tableName, "users");
        EXPECT_FALSE(select.selectAll);
        ASSERT_EQ(select.projection.size(), 2U);
        EXPECT_TRUE(select.hasWhere);
        EXPECT_EQ(select.where.column, "id");
        EXPECT_EQ(select.where.literal.kind, advdb::SqlLiteral::Kind::Int);
    }
}

TEST(SqlParserTest, ReportsSyntaxErrors) {
    const std::string badSql = "SELECT FROM users;";

    advdb::SqlLexer lexer(badSql);
    std::vector<advdb::SqlToken> tokens;
    advdb::SqlParseError error;
    ASSERT_TRUE(lexer.tokenize(tokens, error));

    advdb::SqlParser parser(tokens);
    advdb::SqlStatement stmt;
    EXPECT_FALSE(parser.parse(stmt, error));
    EXPECT_FALSE(error.message.empty());
}

TEST(QueryExecutionTest, ProjectAndFilterPipeline) {
    const std::string dbPath = tempPath("query_exec_project_filter");
    cleanupFile(dbPath);
    std::remove((dbPath + ".users.heap").c_str());

    Database db(dbPath);
    ASSERT_TRUE(db.initialize());

    const advdb::TableSchema schema = makeUsersSchema();
    std::string error;
    ASSERT_TRUE(db.createTable(schema, error));

    ASSERT_TRUE(db.insertRow("users", {advdb::Value::makeInt(1), advdb::Value::makeString("Alice"), advdb::Value::makeString("A")}, error));
    ASSERT_TRUE(db.insertRow("users", {advdb::Value::makeInt(2), advdb::Value::makeString("Bob"), advdb::Value::makeString("B")}, error));
    ASSERT_TRUE(db.insertRow("users", {advdb::Value::makeInt(3), advdb::Value::makeString("Carol"), advdb::Value::makeString("C")}, error));

    advdb::Predicate pred;
    pred.column = "id";
    pred.op = advdb::CompareOp::GT;
    pred.value = advdb::Value::makeInt(1);

    std::vector<advdb::Row> outRows;
    std::vector<advdb::ColumnDefinition> outCols;
    ASSERT_TRUE(db.selectRowsProjected("users", {pred}, {"name"}, outRows, outCols, error)) << error;

    ASSERT_EQ(outCols.size(), 1U);
    EXPECT_EQ(outCols[0].name, "name");
    ASSERT_EQ(outRows.size(), 2U);
    EXPECT_EQ(outRows[0].size(), 1U);
    EXPECT_EQ(outRows[0][0].strVal, "Bob");
    EXPECT_EQ(outRows[1][0].strVal, "Carol");

    cleanupFile(dbPath);
    std::remove((dbPath + ".users.heap").c_str());
}

TEST(QueryExecutionTest, UnknownProjectionColumnFails) {
    const std::string dbPath = tempPath("query_exec_bad_projection");
    cleanupFile(dbPath);

    Database db(dbPath);
    ASSERT_TRUE(db.initialize());

    const advdb::TableSchema schema = makeUsersSchema();
    std::string error;
    ASSERT_TRUE(db.createTable(schema, error));

    std::vector<advdb::Row> outRows;
    std::vector<advdb::ColumnDefinition> outCols;
    EXPECT_FALSE(db.selectRowsProjected("users", {}, {"does_not_exist"}, outRows, outCols, error));
    EXPECT_NE(error.find("Unknown projection column"), std::string::npos);

    cleanupFile(dbPath);
}

// Week 15-16: Advanced SQL Features Tests
TEST(SqlParserTest, ParseJoinClause) {
    // Note: Simple column names without table prefix for MVP
    const std::string sql = "SELECT * FROM users JOIN orders ON id = user_id;";
    
    advdb::SqlLexer lexer(sql);
    std::vector<advdb::SqlToken> tokens;
    advdb::SqlParseError lexErr;
    ASSERT_TRUE(lexer.tokenize(tokens, lexErr));

    advdb::SqlParser parser(tokens);
    advdb::SqlStatement stmt;
    advdb::SqlParseError parseErr;
    ASSERT_TRUE(parser.parse(stmt, parseErr));
    
    const auto& select = std::get<advdb::SqlSelectStatement>(stmt);
    EXPECT_EQ(select.tableName, "users");
    ASSERT_EQ(select.joins.size(), 1U);
    EXPECT_EQ(select.joins[0].joinTable, "orders");
    EXPECT_EQ(select.joins[0].leftColumn, "id");
    EXPECT_EQ(select.joins[0].rightColumn, "user_id");
}

TEST(SqlParserTest, ParseGroupByClause) {
    const std::string sql = "SELECT dept_id FROM employees GROUP BY dept_id;";
    
    advdb::SqlLexer lexer(sql);
    std::vector<advdb::SqlToken> tokens;
    advdb::SqlParseError lexErr;
    ASSERT_TRUE(lexer.tokenize(tokens, lexErr));

    advdb::SqlParser parser(tokens);
    advdb::SqlStatement stmt;
    advdb::SqlParseError parseErr;
    ASSERT_TRUE(parser.parse(stmt, parseErr));
    
    const auto& select = std::get<advdb::SqlSelectStatement>(stmt);
    EXPECT_TRUE(select.hasGroupBy);
    ASSERT_EQ(select.groupBy.columns.size(), 1U);
    EXPECT_EQ(select.groupBy.columns[0], "dept_id");
}

TEST(SqlParserTest, ParseOrderByClause) {
    const std::string sql = "SELECT name FROM users ORDER BY name DESC, id ASC;";
    
    advdb::SqlLexer lexer(sql);
    std::vector<advdb::SqlToken> tokens;
    advdb::SqlParseError lexErr;
    ASSERT_TRUE(lexer.tokenize(tokens, lexErr));

    advdb::SqlParser parser(tokens);
    advdb::SqlStatement stmt;
    advdb::SqlParseError parseErr;
    ASSERT_TRUE(parser.parse(stmt, parseErr));
    
    const auto& select = std::get<advdb::SqlSelectStatement>(stmt);
    EXPECT_TRUE(select.hasOrderBy);
    ASSERT_EQ(select.orderBy.columns.size(), 2U);
    EXPECT_EQ(select.orderBy.columns[0].first, "name");
    EXPECT_EQ(select.orderBy.columns[0].second, advdb::SqlOrderByClause::Direction::Desc);
    EXPECT_EQ(select.orderBy.columns[1].second, advdb::SqlOrderByClause::Direction::Asc);
}

TEST(SqlParserTest, ParseHavingClause) {
    const std::string sql = "SELECT dept_id FROM employees GROUP BY dept_id HAVING COUNT(id) > 5;";
    
    advdb::SqlLexer lexer(sql);
    std::vector<advdb::SqlToken> tokens;
    advdb::SqlParseError lexErr;
    ASSERT_TRUE(lexer.tokenize(tokens, lexErr));

    advdb::SqlParser parser(tokens);
    advdb::SqlStatement stmt;
    advdb::SqlParseError parseErr;
    ASSERT_TRUE(parser.parse(stmt, parseErr));
    
    const auto& select = std::get<advdb::SqlSelectStatement>(stmt);
    EXPECT_TRUE(select.hasHaving);
    EXPECT_EQ(select.having.aggregateFunc, "COUNT");
    EXPECT_EQ(select.having.aggregateColumn, "id");
    EXPECT_EQ(select.having.op, advdb::SqlWhereClause::Op::Gt);
}

TEST(QueryExecutionTest, SortOperatorOrdering) {
    const std::string dbPath = tempPath("query_exec_sort");
    cleanupFile(dbPath);
    std::remove((dbPath + ".scores.heap").c_str());

    Database db(dbPath);
    ASSERT_TRUE(db.initialize());

    advdb::TableSchema schema;
    schema.name = "scores";
    schema.columns = {
        advdb::ColumnDefinition{"player", advdb::ColumnType::Varchar, 64U, false},
        advdb::ColumnDefinition{"points", advdb::ColumnType::Int, 0U, false}
    };

    std::string error;
    ASSERT_TRUE(db.createTable(schema, error));

    ASSERT_TRUE(db.insertRow("scores", {advdb::Value::makeString("Alice"), advdb::Value::makeInt(100)}, error));
    ASSERT_TRUE(db.insertRow("scores", {advdb::Value::makeString("Bob"), advdb::Value::makeInt(200)}, error));
    ASSERT_TRUE(db.insertRow("scores", {advdb::Value::makeString("Carol"), advdb::Value::makeInt(150)}, error));

    // Test SortOperator directly
    advdb::TableHeap heap(dbPath + ".scores.heap");
    ASSERT_TRUE(heap.open());

    advdb::SortOperator::SortKey key{"points", true};  // DESC
    
    advdb::SortOperator sort(std::make_unique<advdb::ScanOperator>(heap, schema), schema, {key});
    
    std::string sortError;
    ASSERT_TRUE(sort.open(sortError));
    
    advdb::Row row;
    std::vector<int> sortedPoints;
    while (sort.next(row, sortError)) {
        if (row.size() >= 2 && row[1].isInt()) {
            sortedPoints.push_back(static_cast<int>(row[1].intVal));
        }
    }
    sort.close();

    ASSERT_EQ(sortedPoints.size(), 3U);
    EXPECT_EQ(sortedPoints[0], 200);  // Highest first (DESC)
    EXPECT_EQ(sortedPoints[1], 150);
    EXPECT_EQ(sortedPoints[2], 100);

    cleanupFile(dbPath);
    std::remove((dbPath + ".scores.heap").c_str());
}

TEST(QueryExecutionTest, AggregateOperatorCount) {
    const std::string dbPath = tempPath("query_exec_aggregate");
    cleanupFile(dbPath);
    std::remove((dbPath + ".sales.heap").c_str());

    Database db(dbPath);
    ASSERT_TRUE(db.initialize());

    advdb::TableSchema schema;
    schema.name = "sales";
    schema.columns = {
        advdb::ColumnDefinition{"id", advdb::ColumnType::Int, 0U, false},
        advdb::ColumnDefinition{"amount", advdb::ColumnType::Int, 0U, false}
    };

    std::string error;
    ASSERT_TRUE(db.createTable(schema, error));

    ASSERT_TRUE(db.insertRow("sales", {advdb::Value::makeInt(1), advdb::Value::makeInt(10)}, error));
    ASSERT_TRUE(db.insertRow("sales", {advdb::Value::makeInt(2), advdb::Value::makeInt(20)}, error));
    ASSERT_TRUE(db.insertRow("sales", {advdb::Value::makeInt(3), advdb::Value::makeInt(30)}, error));

    advdb::TableHeap heap(dbPath + ".sales.heap");
    ASSERT_TRUE(heap.open());

    advdb::AggregateOperator aggr(std::make_unique<advdb::ScanOperator>(heap, schema), 
                                  schema,
                                  advdb::AggregateOperator::AggFunc::Count,
                                  "id");
    
    std::string aggError;
    ASSERT_TRUE(aggr.open(aggError));
    
    advdb::Row result;
    ASSERT_TRUE(aggr.next(result, aggError));
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].intVal, 3);  // COUNT = 3
    
    // Second call should return false (aggregate result already emitted)
    EXPECT_FALSE(aggr.next(result, aggError));
    aggr.close();

    cleanupFile(dbPath);
    std::remove((dbPath + ".sales.heap").c_str());
}

TEST(StatisticsTest, UpdateAndRetrieveTableStats) {
    advdb::Statistics stats;
    
    advdb::ColumnStats col1;
    col1.columnName = "id";
    col1.distinctCount = 100;
    col1.isNumeric = true;
    
    advdb::ColumnStats col2;
    col2.columnName = "name";
    col2.distinctCount = 100;
    col2.isNumeric = false;
    
    advdb::TableStats tableStats;
    tableStats.tableName = "users";
    tableStats.rowCount = 1000;
    tableStats.columnStats = {col1, col2};
    
    stats.updateTableStats(tableStats);
    
    const advdb::TableStats* retrieved = stats.getTableStats("users");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->tableName, "users");
    EXPECT_EQ(retrieved->rowCount, 1000);
    EXPECT_EQ(retrieved->columnStats.size(), 2U);
}

TEST(StatisticsTest, EstimateSelectivityForNumericColumn) {
    advdb::Statistics stats;
    
    advdb::ColumnStats col;
    col.columnName = "age";
    col.distinctCount = 50;
    col.isNumeric = true;
    
    advdb::TableStats tableStats;
    tableStats.tableName = "persons";
    tableStats.rowCount = 5000;
    tableStats.columnStats = {col};
    
    stats.updateTableStats(tableStats);
    
    double selectivity = stats.estimateSelectivity("persons", "age", 25.0);
    EXPECT_GT(selectivity, 0.0);
    EXPECT_LE(selectivity, 0.1);  // Assume 1/50 = 0.02 for uniform distribution
}

TEST(StatisticsTest, EstimateOutputRows) {
    advdb::Statistics stats;
    
    advdb::ColumnStats col;
    col.columnName = "id";
    col.distinctCount = 100;
    col.isNumeric = true;
    
    advdb::TableStats tableStats;
    tableStats.tableName = "orders";
    tableStats.rowCount = 10000;
    tableStats.columnStats = {col};
    
    stats.updateTableStats(tableStats);
    
    long long estimatedRows = stats.estimateOutputRows("orders", 0.1);
    EXPECT_EQ(estimatedRows, 1000);  // 10000 * 0.1 = 1000
}

TEST(StatisticsTest, ListAllTables) {
    advdb::Statistics stats;
    
    advdb::TableStats table1;
    table1.tableName = "users";
    table1.rowCount = 1000;
    
    advdb::TableStats table2;
    table2.tableName = "orders";
    table2.rowCount = 5000;
    
    stats.updateTableStats(table1);
    stats.updateTableStats(table2);
    
    auto tables = stats.listTables();
    EXPECT_EQ(tables.size(), 2U);
    EXPECT_TRUE(std::find(tables.begin(), tables.end(), "users") != tables.end());
    EXPECT_TRUE(std::find(tables.begin(), tables.end(), "orders") != tables.end());
}

TEST(QueryPlannerTest, EstimateScanCostWithoutStats) {
    advdb::Statistics stats;
    advdb::QueryPlanner planner(stats);
    
    // Create a mock scan node
    advdb::PlanNode scanNode;
    scanNode.type = advdb::PlanNodeType::Scan;
    scanNode.detail = "unknown_table";
    
    auto scanNodePtr = std::make_shared<advdb::PlanNode>(scanNode);
    advdb::CostEstimate cost = planner.estimateCost(scanNodePtr);
    
    EXPECT_GT(cost.cpuCost, 0.0);
    EXPECT_EQ(cost.outputRows, 1000);  // Default estimate
}

TEST(QueryPlannerTest, EstimateJoinCost) {
    advdb::Statistics stats;
    advdb::QueryPlanner planner(stats);
    
    advdb::CostEstimate joinCost = planner.estimateJoinCost("left_table", 100, "right_table", 200);
    
    EXPECT_GT(joinCost.cpuCost, 0.0);
    EXPECT_GT(joinCost.outputRows, 0);
    EXPECT_EQ(joinCost.ioCount, 300);  // left + right
}

TEST(QueryPlannerTest, OptimizeJoinOrderWithEmptyJoins) {
    advdb::Statistics stats;
    advdb::QueryPlanner planner(stats);
    
    std::vector<advdb::SqlJoinClause> joins;  // Empty
    auto optimized = planner.optimizeJoinOrder("base_table", joins);
    
    EXPECT_TRUE(optimized.empty());
}

TEST(IndexTest, CreateIndexAndPointLookup) {
    const std::string dbPath = tempPath("index_point_lookup");
    cleanupFile(dbPath);
    std::remove((dbPath + ".users.heap").c_str());
    std::remove((dbPath + ".users.id.idx").c_str());
    std::remove((dbPath + ".indexes.meta").c_str());

    Database db(dbPath);
    ASSERT_TRUE(db.initialize());

    advdb::TableSchema schema;
    schema.name = "users";
    schema.columns = {
        advdb::ColumnDefinition{"id", advdb::ColumnType::Int, 0U, false},
        advdb::ColumnDefinition{"name", advdb::ColumnType::Text, 0U, false}
    };

    std::string error;
    ASSERT_TRUE(db.createTable(schema, error));

    ASSERT_TRUE(db.insertRow("users", {advdb::Value::makeInt(1), advdb::Value::makeString("alice")}, error));
    ASSERT_TRUE(db.insertRow("users", {advdb::Value::makeInt(2), advdb::Value::makeString("bob")}, error));
    ASSERT_TRUE(db.insertRow("users", {advdb::Value::makeInt(3), advdb::Value::makeString("carol")}, error));

    ASSERT_TRUE(db.createIndex("users", "id", error));

    std::vector<advdb::Predicate> predicates{
        advdb::Predicate{"id", advdb::CompareOp::EQ, advdb::Value::makeInt(2)}
    };
    std::vector<advdb::Row> rows;

    ASSERT_TRUE(db.selectRows("users", predicates, rows, error));
    ASSERT_EQ(rows.size(), 1U);
    ASSERT_EQ(rows[0].size(), 2U);
    EXPECT_TRUE(rows[0][0].isInt());
    EXPECT_EQ(rows[0][0].intVal, 2);
    EXPECT_TRUE(rows[0][1].isString());
    EXPECT_EQ(rows[0][1].strVal, "bob");

    cleanupFile(dbPath);
    std::remove((dbPath + ".users.heap").c_str());
    std::remove((dbPath + ".users.id.idx").c_str());
    std::remove((dbPath + ".indexes.meta").c_str());
}

TEST(IndexTest, RangeScanLookup) {
    const std::string dbPath = tempPath("index_range_lookup");
    cleanupFile(dbPath);
    std::remove((dbPath + ".metrics.heap").c_str());
    std::remove((dbPath + ".metrics.score.idx").c_str());
    std::remove((dbPath + ".indexes.meta").c_str());

    Database db(dbPath);
    ASSERT_TRUE(db.initialize());

    advdb::TableSchema schema;
    schema.name = "metrics";
    schema.columns = {
        advdb::ColumnDefinition{"id", advdb::ColumnType::Int, 0U, false},
        advdb::ColumnDefinition{"score", advdb::ColumnType::Int, 0U, false}
    };

    std::string error;
    ASSERT_TRUE(db.createTable(schema, error));
    ASSERT_TRUE(db.insertRow("metrics", {advdb::Value::makeInt(1), advdb::Value::makeInt(10)}, error));
    ASSERT_TRUE(db.insertRow("metrics", {advdb::Value::makeInt(2), advdb::Value::makeInt(20)}, error));
    ASSERT_TRUE(db.insertRow("metrics", {advdb::Value::makeInt(3), advdb::Value::makeInt(30)}, error));
    ASSERT_TRUE(db.insertRow("metrics", {advdb::Value::makeInt(4), advdb::Value::makeInt(40)}, error));

    ASSERT_TRUE(db.createIndex("metrics", "score", error));

    std::vector<advdb::Predicate> predicates{
        advdb::Predicate{"score", advdb::CompareOp::GTE, advdb::Value::makeInt(30)}
    };
    std::vector<advdb::Row> rows;

    ASSERT_TRUE(db.selectRows("metrics", predicates, rows, error));
    ASSERT_EQ(rows.size(), 2U);

    cleanupFile(dbPath);
    std::remove((dbPath + ".metrics.heap").c_str());
    std::remove((dbPath + ".metrics.score.idx").c_str());
    std::remove((dbPath + ".indexes.meta").c_str());
}

TEST(IndexTest, IndexMaintainedAfterInsert) {
    const std::string dbPath = tempPath("index_insert_maintenance");
    cleanupFile(dbPath);
    std::remove((dbPath + ".orders.heap").c_str());
    std::remove((dbPath + ".orders.order_id.idx").c_str());
    std::remove((dbPath + ".indexes.meta").c_str());

    Database db(dbPath);
    ASSERT_TRUE(db.initialize());

    advdb::TableSchema schema;
    schema.name = "orders";
    schema.columns = {
        advdb::ColumnDefinition{"order_id", advdb::ColumnType::Int, 0U, false},
        advdb::ColumnDefinition{"customer", advdb::ColumnType::Text, 0U, false}
    };

    std::string error;
    ASSERT_TRUE(db.createTable(schema, error));
    ASSERT_TRUE(db.insertRow("orders", {advdb::Value::makeInt(10), advdb::Value::makeString("a")}, error));
    ASSERT_TRUE(db.createIndex("orders", "order_id", error));

    ASSERT_TRUE(db.insertRow("orders", {advdb::Value::makeInt(11), advdb::Value::makeString("b")}, error));

    std::vector<advdb::Predicate> predicates{
        advdb::Predicate{"order_id", advdb::CompareOp::EQ, advdb::Value::makeInt(11)}
    };
    std::vector<advdb::Row> rows;

    ASSERT_TRUE(db.selectRows("orders", predicates, rows, error));
    ASSERT_EQ(rows.size(), 1U);
    EXPECT_EQ(rows[0][0].intVal, 11);

    cleanupFile(dbPath);
    std::remove((dbPath + ".orders.heap").c_str());
    std::remove((dbPath + ".orders.order_id.idx").c_str());
    std::remove((dbPath + ".indexes.meta").c_str());
}

