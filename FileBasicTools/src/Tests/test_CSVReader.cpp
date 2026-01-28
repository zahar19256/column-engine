#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include "CSVReader.h"
#include "Row.h"

namespace fs = std::filesystem;

class TempCSVFile {
public:
    TempCSVFile(const std::string& filename, const std::string& content) 
        : path_(fs::current_path() / filename) {
        std::ofstream out(path_);
        out << content;
        out.close();
    }

    ~TempCSVFile() {
        if (fs::exists(path_)) {
            fs::remove(path_);
        }
    }

    std::string GetPath() const {
        return path_.string();
    }

private:
    fs::path path_;
};

TEST(CSVReaderTest, ReadSingleRow) {
    std::string content = "col1,col2,col3\n10,20,30";
    TempCSVFile tempFile("test_row.csv", content);

    CSVReader reader(tempFile.GetPath());

    Row<std::string> row;
    size_t bytesRead = 0;
    
    reader.ReadRowCSV(row, bytesRead);
    
    EXPECT_EQ(row.Size(), 3);
    EXPECT_EQ(row[0], "col1");
    EXPECT_EQ(row[2], "col3");
    EXPECT_GT(bytesRead, 0);
}

TEST(CSVReaderTest, ReadFullTable) {
    std::string content = 
        "name,age\n"
        "Alice,25\n"
        "Bob,30\n";
    TempCSVFile tempFile("test_full.csv", content);

    CSVReader reader(tempFile.GetPath());
    auto table = reader.ReadFullTable();

    ASSERT_EQ(table.size(), 3);

    EXPECT_EQ(table[0][0], "name");
    EXPECT_EQ(table[1][0], "Alice");
    EXPECT_EQ(table[2][1], "30");
}

TEST(CSVReaderTest, CustomDelimiter) {
    std::string content = "id;value\n1;100";
    TempCSVFile tempFile("test_delim.csv", content);

    CSVReader reader(tempFile.GetPath());
    auto table = reader.ReadFullTable(';');

    ASSERT_EQ(table.size(), 2);
    EXPECT_EQ(table[0][0], "id");
    EXPECT_EQ(table[0][1], "value");
    EXPECT_EQ(table[1][0], "1");
    EXPECT_EQ(table[1][1], "100");
}

TEST(CSVReaderTest, ReadChunk_SmallBucket) {
    std::string content = "1,2\n3,4\n5,6\n";
    TempCSVFile tempFile("test_chunk.csv", content);

    size_t small_bucket_size = 1; 
    CSVReader reader(tempFile.GetPath(), small_bucket_size);

    std::vector<Row<std::string>> chunk1 = reader.ReadChunk();
    ASSERT_FALSE(chunk1.empty());
    EXPECT_EQ(chunk1[0][0], "1");
    
    std::vector<Row<std::string>> chunk2 = reader.ReadChunk();
    ASSERT_FALSE(chunk2.empty());
    EXPECT_EQ(chunk2[0][0], "3");
    
    std::vector<Row<std::string>> chunk3 = reader.ReadChunk();
    ASSERT_FALSE(chunk3.empty());
    EXPECT_EQ(chunk3[0][0], "5");
}

TEST(CSVReaderTest, BOM_Handling) {
    std::string content = "\xEF\xBB\xBFheader\ndata";
    TempCSVFile tempFile("test_bom.csv", content);

    CSVReader reader(tempFile.GetPath());
    
    reader.BOMHelper(); 
    
    Row<std::string> row;
    size_t bytes = 0;
    reader.ReadRowCSV(row, bytes);

    EXPECT_EQ(row[0], "header");
}

TEST(CSVReaderTest, EmptyFile) {
    TempCSVFile tempFile("empty.csv", "");
    CSVReader reader(tempFile.GetPath());

    auto table = reader.ReadFullTable();
    EXPECT_TRUE(table.empty());
}

TEST(CSVReaderTest, LargeFile_MultipleBuckets) {
    std::stringstream ss;
    size_t target_size = 4096 * 5;
    int expected_rows = 0;

    while (ss.str().size() < target_size) {
        ss << expected_rows << "," 
           << "payload_data_to_fill_space_for_testing_buckets_" << expected_rows 
           << "\n";
        expected_rows++;
    }

    std::string bigContent = ss.str();
    TempCSVFile tempFile("large_test.csv", bigContent);
    CSVReader reader(tempFile.GetPath(), 4096);
    int rows_read = 0;
    while (true) {
        auto chunk = reader.ReadChunk();
        if (chunk.empty()) {
            break;
        }
        for (const auto& row : chunk) {
            int id = std::stoi(row[0]);
            ASSERT_EQ(id, rows_read) << "Mismatch at row index " << rows_read;
            rows_read++;
        }
    }
    EXPECT_EQ(rows_read, expected_rows);
}

TEST(CSVReaderTest, Escaping_DelimiterInQuotes) {
    std::string content = R"(id,desc,val
1,"comma, separated, value",100
2,normal,200)";
    
    TempCSVFile tempFile("test_delim_esc.csv", content);
    CSVReader reader(tempFile.GetPath());
    
    auto table = reader.ReadFullTable();

    ASSERT_EQ(table.size(), 3); 

    EXPECT_EQ(table[1][1], "comma, separated, value"); 
    EXPECT_EQ(table[1][2], "100");
}

TEST(CSVReaderTest, Escaping_QuotesInQuotes) {
    std::string content = R"(id,text,tag
1,"Text with ""quotes"" inside",ok)";

    TempCSVFile tempFile("test_quotes_esc.csv", content);
    CSVReader reader(tempFile.GetPath());

    auto table = reader.ReadFullTable();

    ASSERT_EQ(table.size(), 2);

    EXPECT_EQ(table[1][1], "Text with \"quotes\" inside");
}

TEST(CSVReaderTest, Escaping_NewlineInQuotes) {
    std::string content = "id,text,status\n"
                          "1,\"Line one\nLine two\",Done";
    
    TempCSVFile tempFile("test_newline_esc.csv", content);
    CSVReader reader(tempFile.GetPath());

    auto table = reader.ReadFullTable();

    ASSERT_EQ(table.size(), 2);

    std::string expectedText = "Line one\nLine two";
    EXPECT_EQ(table[1][1], expectedText);
    EXPECT_EQ(table[1][2], "Done");
}

TEST(CSVReaderTest, Escaping_Complex) {
    std::string content = R"(col1,col2
"",""
"starts with, comma",",ends with comma")";

    TempCSVFile tempFile("test_complex_esc.csv", content);
    CSVReader reader(tempFile.GetPath());

    auto table = reader.ReadFullTable();
    
    ASSERT_EQ(table.size(), 3);

    EXPECT_EQ(table[1][0], "");
    EXPECT_EQ(table[1][1], ""); 

    EXPECT_EQ(table[2][0], "starts with, comma");
    EXPECT_EQ(table[2][1], ",ends with comma");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}