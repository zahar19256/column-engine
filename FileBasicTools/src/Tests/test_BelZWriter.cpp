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

    writer.WriteInt64(std::string("10").data() , 2); 
    EXPECT_EQ(writer.GetOffSet(), 8);

    writer.WriteInt64(std::string("20").data() , 2);
    EXPECT_EQ(writer.GetOffSet(), 16);
}

TEST_F(BelZWriterTest, WriteInt64) {
    {
        BelZWriter writer(csvPath);
        writer.WriteInt64(std::string("1234567890123").data() , 13); 
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
        writer.WriteInt64(std::string("-500").data() , 4);
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
        writer.WriteString(testStr.data() , testStr.size());

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
        writer.WriteData(intVal.data(), intVal.size(), ColumnType::Int64);
        writer.WriteData(strVal.data(), strVal.size(), ColumnType::String);
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
    meta.AddBatchOffset(0);
    meta.AddBatchOffset(1024);
    meta.AddRows(100);
    meta.AddRows(200);
    meta.AddColumnOffset(0);
    meta.AddColumnOffset(1024);
    
    Scheme scheme;
    scheme.Push_Back({"col1", ColumnType::Int64});
    meta.SetScheme(std::move(scheme));

    {
        BelZWriter writer(csvPath);
        writer.WriteInt64(std::string("0").data() , 1); 
        writer.WriteMeta(meta);
        
        EXPECT_GT(writer.GetOffSet(), 8);
    }

    auto data = ReadBinaryFile(belzPath);
    ASSERT_GT(data.size(), 8);

    const uint8_t* ptr = data.data();
    const size_t file_size = data.size();

    uint64_t meta_start = 0;
    std::memcpy(&meta_start, ptr + file_size - sizeof(uint64_t), sizeof(uint64_t));
    ASSERT_LT(meta_start, file_size - sizeof(uint64_t));

    const uint8_t* meta_ptr = ptr + meta_start;
    size_t col_count = 0;
    std::memcpy(&col_count, meta_ptr, sizeof(size_t));
    meta_ptr += sizeof(size_t);
    ASSERT_EQ(col_count, 1u);

    size_t name_len = 0;
    std::memcpy(&name_len, meta_ptr, sizeof(size_t));
    meta_ptr += sizeof(size_t);
    std::string col_name(reinterpret_cast<const char*>(meta_ptr), name_len);
    meta_ptr += name_len;
    EXPECT_EQ(col_name, "col1");

    uint8_t type = 0;
    std::memcpy(&type, meta_ptr, sizeof(uint8_t));
    meta_ptr += sizeof(uint8_t);
    EXPECT_EQ(type, static_cast<uint8_t>(ColumnType::Int64));

    size_t batches_count = 0;
    std::memcpy(&batches_count, meta_ptr, sizeof(size_t));
    meta_ptr += sizeof(size_t);
    ASSERT_EQ(batches_count, 2u);

    size_t batch_offset_0 = 0;
    size_t batch_offset_1 = 0;
    std::memcpy(&batch_offset_0, meta_ptr, sizeof(size_t));
    meta_ptr += sizeof(size_t);
    std::memcpy(&batch_offset_1, meta_ptr, sizeof(size_t));
    meta_ptr += sizeof(size_t);
    EXPECT_EQ(batch_offset_0, 0u);
    EXPECT_EQ(batch_offset_1, 1024u);

    size_t rows_0 = 0;
    size_t rows_1 = 0;
    std::memcpy(&rows_0, meta_ptr, sizeof(size_t));
    meta_ptr += sizeof(size_t);
    std::memcpy(&rows_1, meta_ptr, sizeof(size_t));
    meta_ptr += sizeof(size_t);
    EXPECT_EQ(rows_0, 100u);
    EXPECT_EQ(rows_1, 200u);

    size_t col_offset_0 = 0;
    size_t col_offset_1 = 0;
    std::memcpy(&col_offset_0, meta_ptr, sizeof(size_t));
    meta_ptr += sizeof(size_t);
    std::memcpy(&col_offset_1, meta_ptr, sizeof(size_t));
    EXPECT_EQ(col_offset_0, 0u);
    EXPECT_EQ(col_offset_1, 1024u);
}