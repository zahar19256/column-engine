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