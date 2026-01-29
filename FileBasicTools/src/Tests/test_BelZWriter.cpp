#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include "BelZWriter.h"
#include "MetaData.h"
#include "Scheme.h"
#include <cstring>

namespace fs = std::filesystem;

class BelZWriterTest : public ::testing::Test {
protected:
    std::string csvPath = "test_data.csv";
    std::string belzPath = "test_data.belZ";

    void SetUp() override {
        if (fs::exists(belzPath)) fs::remove(belzPath);
        std::ofstream(csvPath).close();
    }

    void TearDown() override {
        if (fs::exists(belzPath)) fs::remove(belzPath);
        if (fs::exists(csvPath)) fs::remove(csvPath);
    }

    std::vector<uint8_t> ReadBinaryFile(const std::string& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) return {};
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(input)),
                                     std::istreambuf_iterator<char>());
    }
};

TEST_F(BelZWriterTest, FileCreationAndNaming) {
    {
        BelZWriter writer(csvPath);
    }
    
    EXPECT_TRUE(fs::exists(belzPath));
}

TEST_F(BelZWriterTest, OffsetTracking) {
    BelZWriter writer(csvPath);
    
    EXPECT_EQ(writer.GetOffSet(), 0);

    writer.WriteInt64("10"); 
    EXPECT_EQ(writer.GetOffSet(), 8);

    writer.WriteInt64("20");
    EXPECT_EQ(writer.GetOffSet(), 16);
}

TEST_F(BelZWriterTest, WriteInt64) {
    {
        BelZWriter writer(csvPath);
        writer.WriteInt64("1234567890123"); 
    }

    auto data = ReadBinaryFile(belzPath);
    ASSERT_EQ(data.size(), 8);

    int64_t val = 0;
    std::memcpy(&val, data.data(), sizeof(int64_t));
    
    EXPECT_EQ(val, 1234567890123);
}

TEST_F(BelZWriterTest, WriteInt64_Negative) {
    {
        BelZWriter writer(csvPath);
        writer.WriteInt64("-500");
    }

    auto data = ReadBinaryFile(belzPath);
    ASSERT_EQ(data.size(), 8);

    int64_t val = 0;
    std::memcpy(&val, data.data(), sizeof(int64_t));
    EXPECT_EQ(val, -500);
}

TEST_F(BelZWriterTest, WriteString_WithLengthPrefix) {
    std::string testStr = "HelloBelZ";
    size_t expectedLen = testStr.size(); // 9 байт

    {
        BelZWriter writer(csvPath);
        writer.WriteString(testStr);

        EXPECT_EQ(writer.GetOffSet(), sizeof(size_t) + expectedLen);
    }

    auto data = ReadBinaryFile(belzPath);

    ASSERT_EQ(data.size(), sizeof(size_t) + expectedLen);

    size_t storedLen = 0;
    std::memcpy(&storedLen, data.data(), sizeof(size_t));
    EXPECT_EQ(storedLen, expectedLen);
    std::string readStr(data.begin() + sizeof(size_t), data.end());
    EXPECT_EQ(readStr, testStr);
}

TEST_F(BelZWriterTest, WriteData_Dispatcher) {
    std::string intVal = "777";
    std::string strVal = "Text";

    {
        BelZWriter writer(csvPath);
        writer.WriteData(intVal, ColumnType::Int64);
        writer.WriteData(strVal, ColumnType::String);
    }

    auto data = ReadBinaryFile(belzPath);
    size_t expectedTotalSize = 8 + sizeof(size_t) + 4;
    ASSERT_EQ(data.size(), expectedTotalSize);

    int64_t val = 0;
    std::memcpy(&val, data.data(), sizeof(int64_t));
    EXPECT_EQ(val, 777);
    size_t strLen = 0;
    std::memcpy(&strLen, data.data() + 8, sizeof(size_t));
    EXPECT_EQ(strLen, 4);

    std::string readStr(data.begin() + 16, data.end());
    EXPECT_EQ(readStr, "Text");
}

TEST_F(BelZWriterTest, WriteMeta_Integration) {
    MetaData meta;
    meta.AddOffset(0);
    meta.AddOffset(1024);
    meta.AddRows(100);
    
    Scheme scheme;
    scheme.Push_Back({"col1", ColumnType::Int64});
    meta.SetScheme(std::move(scheme));

    {
        BelZWriter writer(csvPath);
        writer.WriteInt64("0"); 
        writer.WriteMeta(meta);
        
        EXPECT_GT(writer.GetOffSet(), 8);
    }

    auto data = ReadBinaryFile(belzPath);
    EXPECT_GT(data.size(), 8); 
}