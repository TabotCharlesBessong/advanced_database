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
    EXPECT_EQ(db.getVersion(), "0.2.0");

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

