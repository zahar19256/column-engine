#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <string>
#include "CSVWriter.h"
#include "Batch.h"
#include "Column.h"

namespace fs = std::filesystem;

std::string ReadFileToString(const std::string& path) {
    std::ifstream t(path);
    if (!t.is_open()) return "";
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

// Важно так как у меня этот класс используется для перегонки из моего формата обратно в CSV я накостылил приписку upd чтобы не ззатирать старый файл.
std::string GetExpectedFilename(const std::string& inputPath) {
    fs::path p(inputPath);
    std::string stem = p.stem().string();
    return (p.parent_path() / (stem + "upd.csv")).string();
}

class CSVWriterTest : public ::testing::Test {
protected:
    std::string testFile = "test_writer.csv";
    std::string expectedFile;

    void SetUp() override {
        expectedFile = GetExpectedFilename(testFile);
        if (fs::exists(expectedFile)) fs::remove(expectedFile);
    }

    void TearDown() override {
        if (fs::exists(expectedFile)) fs::remove(expectedFile);
    }
};

TEST_F(CSVWriterTest, LowLevelPrimitives) {
    {
        CSVWriter writer(testFile);
        writer.WriteInt64(123);
        writer.WriteDelimetr(',');
        writer.WriteString("text");
        writer.WriteDelimetr('\n');
    }
    std::string content = ReadFileToString(expectedFile);
    EXPECT_EQ(content, "123,text\n");
}

TEST_F(CSVWriterTest, StringEscaping) {
    {
        CSVWriter writer(testFile);
        
        writer.WriteString("Hello, World"); 
        writer.WriteDelimetr(',');
        
        writer.WriteString("Says \"Hi\""); 
        
        writer.WriteDelimetr('\n');
    }

    std::string content = ReadFileToString(expectedFile);
    std::string expected = "\"Hello, World\",\"Says \"\"Hi\"\"\"\n";
    
    EXPECT_EQ(content, expected);
}

TEST_F(CSVWriterTest, WriteBatch) {
    auto col1 = std::make_shared<Int64Column>();
    col1->Push_Back(1);
    col1->Push_Back(2);

    auto col2 = std::make_shared<StringColumn>();
    col2->Push_Back("Alice");
    col2->Push_Back("Bob");

    Batch batch;
    Scheme scheme;
    scheme.Push_Back({"id", ColumnType::Int64});
    scheme.Push_Back({"name", ColumnType::String});
    batch.SetScheme(scheme);
    
    batch.AddColumn(col1);
    batch.AddColumn(col2);

    {
        CSVWriter writer(testFile);
        writer.WriteBatch(batch);
    }

    std::string content = ReadFileToString(expectedFile);
    
    std::string expected = "1,Alice\n2,Bob\n";
    EXPECT_EQ(content, expected);
}

TEST_F(CSVWriterTest, WriteBatch_ComplexData) {
    auto col1 = std::make_shared<Int64Column>();
    col1->Push_Back(100);

    auto col2 = std::make_shared<StringColumn>();
    col2->Push_Back("Line1\nLine2,Part3");

    Batch batch;
    Scheme scheme;
    scheme.Push_Back({"num", ColumnType::Int64});
    scheme.Push_Back({"text", ColumnType::String});
    batch.SetScheme(scheme);
    
    batch.AddColumn(col1);
    batch.AddColumn(col2);

    {
        CSVWriter writer(testFile);
        writer.WriteBatch(batch);
    }

    std::string content = ReadFileToString(expectedFile);
    
    std::string expected = "100,\"Line1\nLine2,Part3\"\n";
    EXPECT_EQ(content, expected);
}

TEST_F(CSVWriterTest, CustomDelimiter) {
    {
        CSVWriter writer(testFile);
        writer.WriteInt64(1);
        writer.WriteDelimetr(';');
        writer.WriteString("val");
        writer.WriteDelimetr('\n');
    }

    std::string content = ReadFileToString(expectedFile);
    EXPECT_EQ(content, "1;val\n");
}

TEST_F(CSVWriterTest, WriteBatch_AllTypesWithHeaderAndMaskToExactPath) {
    const std::string output = "test_writer_exact.csv";
    if (fs::exists(output)) fs::remove(output);

    auto int8_col = std::make_shared<Int8Column>();
    int8_col->Push_Back(1);
    int8_col->Push_Back(2);

    auto int16_col = std::make_shared<Int16Column>();
    int16_col->Push_Back(10);
    int16_col->Push_Back(20);

    auto int32_col = std::make_shared<Int32Column>();
    int32_col->Push_Back(100);
    int32_col->Push_Back(200);

    auto int64_col = std::make_shared<Int64Column>();
    int64_col->Push_Back(1000);
    int64_col->Push_Back(2000);

    auto int128_col = std::make_shared<Int128Column>();
    int128_col->Push_Back(static_cast<__int128_t>(1000000000000LL) * 1000000);
    int128_col->Push_Back(-static_cast<__int128_t>(1000000000000LL) * 1000000);

    auto double_col = std::make_shared<DoubleColumn>();
    double_col->Push_Back(1.25);
    double_col->Push_Back(2.5);

    auto string_col = std::make_shared<StringColumn>();
    string_col->Push_Back("plain");
    string_col->Push_Back("quoted,value");

    auto date_col = std::make_shared<DateColumn>();
    date_col->Push_Back(Data::ParseDate("2013-07-15"));
    date_col->Push_Back(Data::ParseDate("2013-07-16"));

    auto timestamp_col = std::make_shared<TimeStampColumn>();
    timestamp_col->Push_Back(Data::ParseTimestamp("2013-07-15 10:47:34"));
    timestamp_col->Push_Back(Data::ParseTimestamp("2013-07-16 11:00:00"));

    Batch batch;
    Scheme scheme;
    scheme.Push_Back({"i8", ColumnType::Int8});
    scheme.Push_Back({"i16", ColumnType::Int16});
    scheme.Push_Back({"i32", ColumnType::Int32});
    scheme.Push_Back({"i64", ColumnType::Int64});
    scheme.Push_Back({"i128", ColumnType::Int128});
    scheme.Push_Back({"d", ColumnType::Double});
    scheme.Push_Back({"s", ColumnType::String});
    scheme.Push_Back({"date", ColumnType::Date});
    scheme.Push_Back({"ts", ColumnType::Timestamp});
    batch.SetScheme(scheme);
    batch.AddColumn(int8_col);
    batch.AddColumn(int16_col);
    batch.AddColumn(int32_col);
    batch.AddColumn(int64_col);
    batch.AddColumn(int128_col);
    batch.AddColumn(double_col);
    batch.AddColumn(string_col);
    batch.AddColumn(date_col);
    batch.AddColumn(timestamp_col);

    boost::dynamic_bitset<> mask(2);
    mask.set(1);
    batch.SetMsk(mask);

    {
        CSVWriter writer(output, CSVWriter::PathMode::ExactPath, true);
        writer.WriteBatch(batch);
        EXPECT_EQ(writer.Rows(), 1);
    }

    std::string content = ReadFileToString(output);
    std::string expected =
        "i8,i16,i32,i64,i128,d,s,date,ts\n"
        "2,20,200,2000,-1000000000000000000,2.5,\"quoted,value\",2013-07-16,2013-07-16 11:00:00\n";
    EXPECT_EQ(content, expected);

    if (fs::exists(output)) fs::remove(output);
}
