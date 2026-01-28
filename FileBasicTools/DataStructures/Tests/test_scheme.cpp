#include <gtest/gtest.h>
#include "Scheme.h"
#include <string>


TEST(SchemeUtils, DatumConvertor_Logic) {
    EXPECT_EQ(DatumConvertor("int64"), ColumnType::Int64); 
    
    EXPECT_EQ(DatumConvertor("string"), ColumnType::String);
    
    EXPECT_EQ(DatumConvertor("GOIDA"), ColumnType::Unknown);
    EXPECT_EQ(DatumConvertor("SVO"), ColumnType::Unknown);
}

TEST(SchemeTest, EmptyState) {
    Scheme scheme;
    EXPECT_EQ(scheme.Size(), 0);
    
    EXPECT_NO_THROW(scheme.GetData());
}

TEST(SchemeTest, PushBack_And_Size) {
    Scheme scheme;
    
    SchemeNode node1{"id", ColumnType::Int64};
    scheme.Push_Back(node1);
    
    EXPECT_EQ(scheme.Size(), 1);
    
    scheme.Push_Back(SchemeNode{"name", ColumnType::String}); // rvalue
    
    EXPECT_EQ(scheme.Size(), 2);
}

TEST(SchemeTest, Accessors_ByIndex) {
    Scheme scheme;
    scheme.Push_Back({"age", ColumnType::Int64});
    scheme.Push_Back({"email", ColumnType::String});

    EXPECT_EQ(scheme.GetName(0), "age");
    EXPECT_EQ(scheme.GetType(0), ColumnType::Int64);

    EXPECT_EQ(scheme.GetName(1), "email");
    EXPECT_EQ(scheme.GetType(1), ColumnType::String);
}

TEST(SchemeTest, GetInfo_ReturnsRef) {
    Scheme scheme;
    scheme.Push_Back({"score", ColumnType::Int64});

    const SchemeNode& node = scheme.GetInfo(0);
    
    EXPECT_EQ(node.name, "score");
    EXPECT_EQ(node.type, ColumnType::Int64);
}

TEST(SchemeTest, GetData_RawPointer) {
    Scheme scheme;
    scheme.Push_Back({"col1", ColumnType::Int64});
    scheme.Push_Back({"col2", ColumnType::String});

    const SchemeNode* raw_data = scheme.GetData();
    
    ASSERT_NE(raw_data, nullptr);

    EXPECT_EQ(raw_data[0].name, "col1");
    EXPECT_EQ(raw_data[0].type, ColumnType::Int64);
    
    EXPECT_EQ(raw_data[1].name, "col2");
    EXPECT_EQ(raw_data[1].type, ColumnType::String);
}

TEST(SchemeTest, Clear) {
    Scheme scheme;
    scheme.Push_Back({"temp", ColumnType::Unknown});
    
    ASSERT_EQ(scheme.Size(), 1);
    
    scheme.Clear();
    
    EXPECT_EQ(scheme.Size(), 0);
}

TEST(SchemeTest, OrderIsPreserved) {
    Scheme scheme;
    scheme.Push_Back({"first", ColumnType::Int64});
    scheme.Push_Back({"second", ColumnType::Int64});
    scheme.Push_Back({"third", ColumnType::Int64});

    EXPECT_EQ(scheme.GetName(0), "first");
    EXPECT_EQ(scheme.GetName(1), "second");
    EXPECT_EQ(scheme.GetName(2), "third");
}
