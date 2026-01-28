#include <gtest/gtest.h>
#include "Row.h"
#include <string>
#include <vector>

TEST(RowTest, Constructor_Default) {
    Row<int> row;
    EXPECT_TRUE(row.Empty());
    EXPECT_EQ(row.Size(), 0);
}

TEST(RowTest, Add_Elements) {
    Row<int> row;
    
    int x = 10;
    row.Add(x);
    row.Add(20);

    EXPECT_EQ(row.Size(), 2);
    EXPECT_FALSE(row.Empty());
    EXPECT_EQ(row[0], 10);
    EXPECT_EQ(row[1], 20);
}

TEST(RowTest, Access_And_Modify) {
    Row<std::string> row;
    row.Add("hello");
    
    EXPECT_EQ(row[0], "hello");

    row[0] = "world";
    EXPECT_EQ(row[0], "world");
}

TEST(RowTest, Clear) {
    Row<int> row;
    row.Add(1);
    row.Add(2);
    
    row.Clear();
    
    EXPECT_TRUE(row.Empty());
    EXPECT_EQ(row.Size(), 0);
}

TEST(RowTest, GetData_ReturnsCopy) {
    Row<int> row;
    row.Add(100);
    row.Add(200);

    std::vector<int> data = row.GetData();

    ASSERT_EQ(data.size(), 2);
    EXPECT_EQ(data[0], 100);
    
    EXPECT_EQ(row.Size(), 2);
    EXPECT_FALSE(row.Empty());
}

TEST(RowTest, ExtractData_MovesData) {
    Row<int> row;
    row.Add(100);
    row.Add(200);

    std::vector<int> data = row.ExtractData();

    ASSERT_EQ(data.size(), 2);
    EXPECT_EQ(data[0], 100);

    EXPECT_EQ(row.Size(), 0);
    EXPECT_TRUE(row.Empty());
}

TEST(RowTest, CopyConstructor) {
    Row<int> original;
    original.Add(5);

    Row<int> copy = original;

    EXPECT_EQ(copy.Size(), 1);
    EXPECT_EQ(copy[0], 5);
    EXPECT_EQ(original.Size(), 1);
}

TEST(RowTest, MoveConstructor) {
    Row<std::string> original;
    original.Add("data");

    Row<std::string> moved_to = std::move(original);

    EXPECT_EQ(moved_to.Size(), 1);
    EXPECT_EQ(moved_to[0], "data");

    EXPECT_EQ(original.Size(), 0); 
}


TEST(RowTest, ComplexTypes) {
    Row<std::string> row;
    std::string s = "test";
    
    row.Add(s);
    row.Add(std::move(s));
    row.Add("literal");

    EXPECT_EQ(row.Size(), 3);
    EXPECT_EQ(row[0], "test");
    EXPECT_EQ(row[1], "test");
    EXPECT_EQ(row[2], "literal");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}