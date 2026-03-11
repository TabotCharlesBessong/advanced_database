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
}

} // namespace

TEST(DatabaseTest, Initialization) {
    Database db("test.db");
    EXPECT_TRUE(db.initialize());
    EXPECT_EQ(db.getVersion(), "0.1.0");
    cleanupFile("test.db");
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