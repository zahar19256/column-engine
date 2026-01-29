#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
#include <string>

#include "BelZConvertor.h"
#include "CSVConvertor.h"
#include "CSVReader.h"

namespace fs = std::filesystem;

class BelZIntegrityTest : public ::testing::Test {
protected:
    std::string baseName = "test_integrity";
    std::string csvInput = baseName + ".csv";
    std::string schemePath = baseName + "_scheme.csv";
    std::string belzPath = baseName + ".belZ"; 
    // Предполагаем, что writer делает так: name.belZ -> stem(name) + "upd.csv"
    std::string csvOutput = baseName + "upd.csv"; 

    void TearDown() override {
        if (fs::exists(csvInput)) fs::remove(csvInput);
        if (fs::exists(schemePath)) fs::remove(schemePath);
        if (fs::exists(belzPath)) fs::remove(belzPath);
        if (fs::exists(csvOutput)) fs::remove(csvOutput);
    }

    void CreateScheme(const std::string& content) {
        std::ofstream out(schemePath);
        out << content;
        out.close();
    }

    // ХЕЛПЕР: Считает количество каждого символа в ПОЛЕЗНЫХ ДАННЫХ
    std::map<char, size_t> GetContentCharCounts(const std::string& path) {
        std::map<char, size_t> counts;
        CSVReader reader(path);
        
        // Читаем батчами, чтобы не забивать память, если файл огромный
        while (true) {
            auto chunk = reader.ReadChunk();
            if (chunk.empty()) break;

            for (const auto& row : chunk) {
                // Проходим по всем колонкам строки
                for (size_t i = 0; i < row.Size(); ++i) {
                    const std::string& cell = row[i];
                    for (char c : cell) {
                        counts[c]++;
                    }
                }
            }
        }
        return counts;
    }

    // Запускает полный цикл: CSV -> BelZ -> CSV
    void RunPipeline() {
        // 1. CSV -> BelZ
        CSVConvertor toBelZ;
        toBelZ.MakeBelZFormat(csvInput, schemePath);
        ASSERT_TRUE(fs::exists(belzPath)) << "Failed to create .belZ file";

        // 2. BelZ -> CSV
        BelZConvertor toCSV;
        toCSV.MakeCSV(belzPath);
        ASSERT_TRUE(fs::exists(csvOutput)) << "Failed to create output CSV file";
    }
};

// 1. ТЕСТ: Большой файл со случайными данными (Char Frequency Check)
TEST_F(BelZIntegrityTest, LargeFile_CharHistogram) {
    // Схема
    CreateScheme("id,int64\ndata,string\n");

    // Генерируем 5000 строк (~100-150 КБ)
    // Используем разные символы, чтобы гистограмма была интересной
    {
        std::ofstream out(csvInput);
        for (int i = 0; i < 5000; ++i) {
            out << i << ",random_data_with_numbers_" << (i * 7) % 100 << "_end\n";
        }
    }

    // Запускаем конвертацию туда-сюда
    RunPipeline();

    // Считаем символы
    auto mapInput = GetContentCharCounts(csvInput);
    auto mapOutput = GetContentCharCounts(csvOutput);

    // Сравниваем карты
    ASSERT_EQ(mapInput.size(), mapOutput.size()) << "Number of unique characters differs";
    
    for (auto const& [key, val] : mapInput) {
        EXPECT_EQ(mapOutput[key], val) 
            << "Count mismatch for character '" << key << "'";
    }
}

// 2. ТЕСТ: Спецсимволы и экранирование (Quotes & Newlines)
// Проверяем, что сложная структура не ломается при двойной конвертации
TEST_F(BelZIntegrityTest, SpecialCharacters_Hardcore) {
    CreateScheme("id,int64\ntext,string\n");

    {
        std::ofstream out(csvInput);
        // Строка 1: Простой текст
        out << "1,Simple\n";
        // Строка 2: Текст с запятой (должен быть в кавычках в CSV)
        out << "2,\"Comma, separated\"\n";
        // Строка 3: Текст с кавычками (должны удвоиться)
        out << "3,\"Quote \"\"inside\"\"\"\n";
        // Строка 4: Перенос строки
        out << "4,\"Line1\nLine2\"\n";
        // Строка 5: Пустая строка
        out << "5,\n"; 
    }

    RunPipeline();

    auto mapInput = GetContentCharCounts(csvInput);
    auto mapOutput = GetContentCharCounts(csvOutput);

    // Сравнение карт покажет, не потерялись ли данные внутри кавычек
    for (auto const& [key, val] : mapInput) {
        EXPECT_EQ(mapOutput[key], val) 
            << "Mismatch for char '" << key << "' with special characters dataset";
    }

    // Дополнительно: Точечная проверка значений через Reader, чтобы убедиться в порядке
    CSVReader readerOut(csvOutput);
    auto table = readerOut.ReadFullTable();
    ASSERT_EQ(table.size(), 5); // header не пишется обычно, или +1 если есть
    
    // Проверка переноса строки (индекс 3, колонка 1)
    // Внимание: индексы зависят от того, есть ли заголовок в выходном файле. 
    // BelZConvertor пишет Batch без заголовка (обычно).
    EXPECT_EQ(table[3][1], "Line1\nLine2"); 
    EXPECT_EQ(table[2][1], "Quote \"inside\"");
}

// 3. ТЕСТ: Очень длинные строки (стресс-тест буфера)
TEST_F(BelZIntegrityTest, VeryLongStrings) {
    CreateScheme("id,int64\nblob,string\n");

    std::string longString(10000, 'A'); // 10 КБ одна строка
    std::string longStringB(5000, 'B'); // 5 КБ

    {
        std::ofstream out(csvInput);
        out << "1," << longString << "\n";
        out << "2," << longStringB << "\n";
    }

    RunPipeline();

    auto mapInput = GetContentCharCounts(csvInput);
    auto mapOutput = GetContentCharCounts(csvOutput);

    EXPECT_EQ(mapInput['A'], 10000);
    EXPECT_EQ(mapOutput['A'], 10000);
    EXPECT_EQ(mapInput['B'], 5000);
    EXPECT_EQ(mapOutput['B'], 5000);
}

// 4. ТЕСТ: Только числа (Int64 only)
TEST_F(BelZIntegrityTest, OnlyIntegers) {
    CreateScheme("col1,int64\ncol2,int64\n");

    {
        std::ofstream out(csvInput);
        out << "100,200\n";
        out << "-50,0\n";
        out << "9223372036854775807,-9223372036854775807\n"; // Max/Min int64 (почти)
    }

    RunPipeline();

    // Сверяем цифры
    auto mapInput = GetContentCharCounts(csvInput);
    auto mapOutput = GetContentCharCounts(csvOutput);
    
    EXPECT_EQ(mapInput['9'], mapOutput['9']);
    EXPECT_EQ(mapInput['-'], mapOutput['-']);
}