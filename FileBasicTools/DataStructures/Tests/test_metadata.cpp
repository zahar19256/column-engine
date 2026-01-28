#include <gtest/gtest.h>
#include "MetaData.h"
#include <vector>

TEST(MetaDataTest, EmptyOnStart) {
    MetaData md;
    
    EXPECT_EQ(md.Size(), 0);
    EXPECT_EQ(md.CountOfBatches(), 0);
    
    EXPECT_TRUE(md.GetOffsets().empty());
    EXPECT_TRUE(md.GetRows().empty());

    EXPECT_EQ(md.GetScheme().Size(), 0);
}

TEST(MetaDataTest, Offsets_AddAndRetrieve) {
    MetaData md;
    
    md.AddOffset(0);
    md.AddOffset(1024);
    md.AddOffset(2048);
    EXPECT_EQ(md.GetOffset(0), 0);
    EXPECT_EQ(md.GetOffset(1), 1024);
    EXPECT_EQ(md.GetOffset(2), 2048);
    
    const std::vector<size_t>& vec = md.GetOffsets();
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

TEST(MetaDataTest, BatchCounts) {
    MetaData md;
    md.AddOffset(100);
    md.AddRows(50);
    
    size_t size = md.Size();
    size_t batches = md.CountOfBatches();
    
    EXPECT_EQ(size, batches);
    EXPECT_GT(size, 0);
}

TEST(MetaDataTest, Clear) {
    MetaData md;

    md.AddOffset(100);
    md.AddRows(50);
    
    Scheme s;
    s.Push_Back({"test", ColumnType::Int64});
    md.SetScheme(std::move(s));
    
    ASSERT_EQ(md.GetOffsets().size(), 1);
    ASSERT_EQ(md.GetScheme().Size(), 1);

    md.Clear();

    EXPECT_EQ(md.Size(), 0);
    EXPECT_EQ(md.CountOfBatches(), 0);
    EXPECT_TRUE(md.GetOffsets().empty());
    EXPECT_TRUE(md.GetRows().empty());

    EXPECT_EQ(md.GetScheme().Size(), 0); 
}