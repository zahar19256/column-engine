#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>

#include "BelZConvertor.h"
#include "CSVConvertor.h"
#include "CSVReader.h" // Чтобы сравнивать CSV логически, а не побайтово

namespace fs = std::filesystem;

class BelZConvertorTest : public ::testing::Test {
protected:
    std::string baseName = "test_cycle";
    std::string csvInput = baseName + ".csv";
    std::string schemePath = baseName + "_scheme.csv";
    std::string belzPath = baseName + ".belZ"; 
    // Имя выходного файла зависит от CSVWriter. 
    // Предполагаем логику: stem(test_cycle.belZ) + "upd.csv" -> test_cycleupd.csv
    std::string csvOutput = baseName + "upd.csv"; 

    void TearDown() override {
        // Чистим все файлы
        if (fs::exists(csvInput)) fs::remove(csvInput);
        if (fs::exists(schemePath)) fs::remove(schemePath);
        if (fs::exists(belzPath)) fs::remove(belzPath);
        if (fs::exists(csvOutput)) fs::remove(csvOutput);
    }

    // Хелпер: Создает схему (id,Int64 и name,String)
    void CreateScheme() {
        std::ofstream out(schemePath);
        out << "id,int64\n";
        out << "name,string\n";
        out.close();
    }

    // Хелпер: Сравнивает два CSV файла ЛОГИЧЕСКИ (парсит их и сверяет ячейки)
    // Это лучше, чем побайтовое сравнение, так как форматирование (кавычки) может отличаться.
    void CompareCSVFiles(const std::string& path1, const std::string& path2) {
        CSVReader r1(path1);
        auto table1 = r1.ReadFullTable();

        CSVReader r2(path2);
        auto table2 = r2.ReadFullTable();

        ASSERT_EQ(table1.size(), table2.size()) 
            << "Row count mismatch between original and converted CSV";

        for (size_t i = 0; i < table1.size(); ++i) {
            ASSERT_EQ(table1[i].Size(), table2[i].Size()) 
                << "Column count mismatch at row " << i;
            
            for (size_t j = 0; j < table1[i].Size(); ++j) {
                EXPECT_EQ(table1[i][j], table2[i][j]) 
                    << "Value mismatch at row " << i << ", col " << j;
            }
        }
    }
};

// 1. Round-Trip Тест: Маленький файл
TEST_F(BelZConvertorTest, RoundTrip_Small) {
    CreateScheme();
    
    // 1. Создаем исходный CSV (2 строки)
    {
        std::ofstream out(csvInput);
        out << "10,Alice\n";
        out << "20,Bob\n";
    }

    // 2. CSV -> BelZ
    CSVConvertor toBelZ;
    toBelZ.MakeBelZFormat(csvInput, schemePath);
    
    ASSERT_TRUE(fs::exists(belzPath)) << "Intermediate .belZ file not created";

    // 3. BelZ -> CSV (Тестируемый класс)
    BelZConvertor toCSV;
    toCSV.MakeCSV(belzPath);

    ASSERT_TRUE(fs::exists(csvOutput)) 
        << "Final CSV file not created. Expected: " << csvOutput;

    // 4. Сравниваем исходный и конечный CSV
    CompareCSVFiles(csvInput, csvOutput);
}

// 2. Round-Trip Тест: Большой файл (Много батчей)
TEST_F(BelZConvertorTest, RoundTrip_Large) {
    CreateScheme();

    // 1. Генерируем большой CSV (~50-100 КБ)
    // Чтобы гарантировать несколько батчей чтения и записи
    size_t rows_count = 2000;
    {
        std::ofstream out(csvInput);
        for (size_t i = 0; i < rows_count; ++i) {
            // id, "long_string_..."
            out << i << ",value_string_for_testing_batching_" << i << "\n";
        }
    }

    // 2. CSV -> BelZ
    CSVConvertor toBelZ;
    toBelZ.MakeBelZFormat(csvInput, schemePath);
    
    ASSERT_TRUE(fs::exists(belzPath));

    // 3. BelZ -> CSV
    BelZConvertor toCSV;
    toCSV.MakeCSV(belzPath);

    ASSERT_TRUE(fs::exists(csvOutput));

    // 4. Сравнение
    // Для большого файла ReadFullTable может быть медленным, 
    // но для теста в 2000 строк это мгновенно.
    CompareCSVFiles(csvInput, csvOutput);
}

// 3. Тест на пустой файл
TEST_F(BelZConvertorTest, EmptyFile) {
    CreateScheme();
    // Пустой CSV
    std::ofstream(csvInput).close();

    // CSV -> BelZ (создаст файл только с метаданными)
    CSVConvertor toBelZ;
    toBelZ.MakeBelZFormat(csvInput, schemePath);

    // BelZ -> CSV
    BelZConvertor toCSV;
    toCSV.MakeCSV(belzPath);

    // Проверяем результат
    CSVReader reader(csvOutput);
    auto table = reader.ReadFullTable();
    EXPECT_EQ(table.size(), 0) << "Resulting CSV should be empty";
}