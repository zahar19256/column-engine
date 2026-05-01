#include <gtest/gtest.h>
#include "MetaData.h"
#include <vector>

TEST(MetaDataTest, EmptyOnStart) {
    MetaData md;

    EXPECT_EQ(md.BatchesCount(), 0);
    EXPECT_EQ(md.ColumnsCount(), 0);

    EXPECT_TRUE(md.GetBatchOffsets().empty());
    EXPECT_TRUE(md.GetColumnOffsets().empty());
    EXPECT_TRUE(md.GetRows().empty());
    EXPECT_EQ(md.GetScheme().Size(), 0);
}

TEST(MetaDataTest, BatchOffsetsAddAndRetrieve) {
    MetaData md;

    md.AddBatchOffset(0);
    md.AddBatchOffset(1024);
    md.AddBatchOffset(2048);
    EXPECT_EQ(md.GetBatchOffset(0), 0);
    EXPECT_EQ(md.GetBatchOffset(1), 1024);
    EXPECT_EQ(md.GetBatchOffset(2), 2048);

    const std::vector<size_t>& vec = md.GetBatchOffsets();
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[1], 1024);
}

TEST(MetaDataTest, Rows_AddAndRetrieve) {
    MetaData md;
    
    md.AddRows(100);
    md.AddRows(55);
    
    EXPECT_EQ(md.GetRow(0), 100);
    EXPECT_EQ(md.GetRow(1), 55);
    
    const std::vector<size_t>& vec = md.GetRows();
    ASSERT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], 100);
}

TEST(MetaDataTest, Scheme_SetAndGet) {
    MetaData md;

    Scheme s;
    s.Push_Back({"col1", ColumnType::Int64});
    s.Push_Back({"col2", ColumnType::String});

    md.SetScheme(std::move(s));

    const Scheme& stored_scheme = md.GetScheme();
    ASSERT_EQ(stored_scheme.Size(), 2);
    EXPECT_EQ(stored_scheme.GetName(0), "col1");
    EXPECT_EQ(stored_scheme.GetName(1), "col2");

    EXPECT_EQ(s.Size(), 0); 
}

TEST(MetaDataTest, ColumnOffsetsFlatLayout_AddAndRetrieve) {
    MetaData md;

    Scheme s;
    s.Push_Back({"id", ColumnType::Int64});
    s.Push_Back({"name", ColumnType::String});
    md.SetScheme(std::move(s));

    // 2 батча x 2 колонки => 4 оффсета в плоском массиве:
    // [b0.c0, b0.c1, b1.c0, b1.c1]
    md.AddBatchOffset(0);
    md.AddBatchOffset(1000);
    md.AddColumnOffset(10);
    md.AddColumnOffset(20);
    md.AddColumnOffset(1010);
    md.AddColumnOffset(1020);

    EXPECT_EQ(md.BatchesCount(), 2);
    EXPECT_EQ(md.ColumnsCount(), 4);

    EXPECT_EQ(md.GetColumnOffset(0, 0), 10);
    EXPECT_EQ(md.GetColumnOffset(0, 1), 20);
    EXPECT_EQ(md.GetColumnOffset(1, 0), 1010);
    EXPECT_EQ(md.GetColumnOffset(1, 1), 1020);
}

TEST(MetaDataTest, OutOfRangeAccessThrows) {
    MetaData md;

    Scheme s;
    s.Push_Back({"id", ColumnType::Int64});
    md.SetScheme(std::move(s));

    md.AddBatchOffset(0);
    md.AddRows(1);
    md.AddColumnOffset(0);

    EXPECT_THROW(md.GetBatchOffset(1), std::runtime_error);
    EXPECT_THROW(md.GetRow(1), std::runtime_error);
    EXPECT_THROW(md.GetColumnOffset(1, 0), std::runtime_error);
}

TEST(MetaDataTest, Clear) {
    MetaData md;

    md.AddBatchOffset(100);
    md.AddRows(50);
    md.AddColumnOffset(120);

    Scheme s;
    s.Push_Back({"test", ColumnType::Int64});
    md.SetScheme(std::move(s));

    ASSERT_EQ(md.GetBatchOffsets().size(), 1);
    ASSERT_EQ(md.GetColumnOffsets().size(), 1);
    ASSERT_EQ(md.GetScheme().Size(), 1);

    md.Clear();

    EXPECT_EQ(md.BatchesCount(), 0);
    EXPECT_EQ(md.ColumnsCount(), 0);
    EXPECT_TRUE(md.GetBatchOffsets().empty());
    EXPECT_TRUE(md.GetColumnOffsets().empty());
    EXPECT_TRUE(md.GetRows().empty());

    EXPECT_EQ(md.GetScheme().Size(), 0); 
}