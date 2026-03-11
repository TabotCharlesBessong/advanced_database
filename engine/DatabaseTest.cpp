#include <gtest/gtest.h>
#include "Database.h"

TEST(DatabaseTest, Initialization) {
    Database db("test.db");
    EXPECT_TRUE(db.initialize());
    EXPECT_EQ(db.getVersion(), "0.1.0");
}