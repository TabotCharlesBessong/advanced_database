#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

#include "BufferPool.h"
#include "Database.h"
#include "FileManager.h"
#include "Page.h"

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