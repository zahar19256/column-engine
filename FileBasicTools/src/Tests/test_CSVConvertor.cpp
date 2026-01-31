#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstring>

#include "CSVConvertor.h"
namespace fs = std::filesystem;

class CSVConvertorTest : public ::testing::Test {
protected:
    std::string csvPath = "data.csv";
    std::string schemePath = "schema.txt"; // Или .json, зависит от вашего парсера
    std::string belzPath = "data.belZ";

    void TearDown() override {
        if (fs::exists(csvPath)) fs::remove(csvPath);
        if (fs::exists(schemePath)) fs::remove(schemePath);
        if (fs::exists(belzPath)) fs::remove(belzPath);
    }

    // Хелпер для создания CSV
    void CreateDummyCSV() {
        std::ofstream out(csvPath);
        // id (Int64), name (String)
        out << "10,Alice\n";
        out << "20,Bob\n";
        out.close();
    }

    // Хелпер для создания Схемы
    // ВНИМАНИЕ: Подставьте сюда тот формат, который умеет читать ваш GetScheme()
    void CreateDummyScheme() {
        std::ofstream out(schemePath);
        // Пример простого формата: "Имя Тип"
        // Если у вас JSON, напишите: { "columns": [...] }
        out << "id,int64\n";
        out << "name,string\n";
        out.close();
    }

    // Хелпер для чтения бинарного файла
    std::vector<uint8_t> ReadBytes() {
        std::ifstream input(belzPath, std::ios::binary);
        if (!input.is_open()) return {};
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(input)),
                                     std::istreambuf_iterator<char>());
    }
};

TEST_F(CSVConvertorTest, EndToEnd_TranspositionCheck) {
    // 1. Подготовка файлов
    CreateDummyCSV();
    CreateDummyScheme();

    // 2. Запуск конвертации
    CSVConvertor convertor;
    
    // Этот метод должен:
    // 1. Прочитать схему.
    // 2. Прочитать CSV.
    // 3. Создать data.belZ.
    // 4. Записать данные ПО КОЛОНКАМ (транспонированно).
    convertor.MakeBelZFormat(csvPath, schemePath);

    // 3. Проверка существования файла
    ASSERT_TRUE(fs::exists(belzPath)) << "Output .belZ file was not created";

    // 4. Проверка содержимого (Самая важная часть)
    auto data = ReadBytes();
    
    // Мы ожидаем, что CSVConvertor считал чанк (обе строки) и записал их транспонированно.
    // Порядок в файле (до метаданных) должен быть таким:
    // [Колонка 1 (id)]: 10, 20
    // [Колонка 2 (name)]: "Alice", "Bob"
    
    const uint8_t* ptr = data.data();

    // --- Проверяем Колонку 1 (Int64) ---
    // Сначала должно идти число 10
    int64_t val1 = 0;
    std::memcpy(&val1, ptr, sizeof(int64_t));
    ptr += sizeof(int64_t);
    EXPECT_EQ(val1, 10);

    // Сразу за ним должно идти число 20 (а НЕ строка "Alice", если транспонирование работает)
    int64_t val2 = 0;
    std::memcpy(&val2, ptr, sizeof(int64_t));
    ptr += sizeof(int64_t);
    EXPECT_EQ(val2, 20);

    // --- Проверяем Колонку 2 (String) ---
    // Строки пишутся как [Length][Data] (исходя из тестов BelZWriter)
    
    // Строка 1: "Alice"
    size_t len1 = 0;
    std::memcpy(&len1, ptr, sizeof(size_t));
    ptr += sizeof(size_t);
    EXPECT_EQ(len1, 5); // Alice length

    std::string str1(ptr, ptr + len1);
    ptr += len1;
    EXPECT_EQ(str1, "Alice");

    // Строка 2: "Bob"
    size_t len2 = 0;
    std::memcpy(&len2, ptr, sizeof(size_t));
    ptr += sizeof(size_t);
    EXPECT_EQ(len2, 3); // Bob length

    std::string str2(ptr, ptr + len2);
    ptr += len2;
    EXPECT_EQ(str2, "Bob");
    
    // После данных должны идти Метаданные (Footer), но их мы детально проверяли в BelZWriter.
    // Главное, что мы подтвердили транспонирование: сначала все ID, потом все Имена.
}

// Тест на случай, если файла схемы нет
TEST_F(CSVConvertorTest, MissingSchemeFile) {
    CreateDummyCSV();
    // Схему НЕ создаем

    CSVConvertor convertor;
    // Ожидаем, что метод выбросит исключение (например, runtime_error),
    // так как не сможет открыть файл схемы.
    EXPECT_THROW(convertor.MakeBelZFormat(csvPath, "missing_scheme.txt"), std::exception);
}

// Тест на пустой CSV
TEST_F(CSVConvertorTest, EmptyCSV) {
    std::ofstream(csvPath).close(); // Пустой файл
    CreateDummyScheme();

    CSVConvertor convertor;
    convertor.MakeBelZFormat(csvPath, schemePath);

    ASSERT_TRUE(fs::exists(belzPath));
    auto data = ReadBytes();
    
    // Файл должен содержать только метаданные (Footer), так как данных нет.
    // Размер должен быть > 0 (так как метаданные занимают место).
    EXPECT_GT(data.size(), 0);
}

// 4. Тест большого файла (> 4096 байт), проверка многократного сброса чанков
TEST_F(CSVConvertorTest, LargeFile_MultipleBatches) {
    // 1. Создаем Схему
    CreateDummyScheme(); // id,Int64 и name,String

    // 2. Генерируем большой CSV
    // Нам нужно пробить барьер в 4096 байт.
    // Пусть одна строка будет ~50 байт. 1000 строк = 50 КБ.
    size_t total_rows_expected = 100000;
    
    {
        std::ofstream out(csvPath);
        std::string padding(40, 'x'); // Длинная строка "xxxxxxxx..."
        
        for (size_t i = 0; i < total_rows_expected; ++i) {
            // Формат: id,name
            // Пример: 0,row_0_xxxxxxxxxxxxxxxxxxxx...
            out << i << ",row_" << i << "_" << padding << "\n";
        }
    }

    // 3. Запускаем конвертацию
    CSVConvertor convertor;
    convertor.MakeBelZFormat(csvPath, schemePath);

    ASSERT_TRUE(fs::exists(belzPath));

    // 4. Валидация через разбор Метаданных (Footer)
    // Мы не будем парсить весь файл (это долго писать), но проверим "подвал",
    // чтобы убедиться, что записалось нужное количество строк и батчей.
    
    auto data = ReadBytes();
    size_t fileSize = data.size();
    ASSERT_GT(fileSize, sizeof(uint64_t)); // Файл не пустой

    const uint8_t* ptr = data.data();

    // А. Читаем Offset начала метаданных (последние 8 байт)
    uint64_t metaStart = 0;
    std::memcpy(&metaStart, ptr + fileSize - sizeof(uint64_t), sizeof(uint64_t));
    
    // Переходим к началу метаданных
    ASSERT_LT(metaStart, fileSize);
    const uint8_t* metaPtr = ptr + metaStart;

    // Б. Читаем col_count
    size_t colCount = 0;
    std::memcpy(&colCount, metaPtr, sizeof(size_t));
    metaPtr += sizeof(size_t);
    ASSERT_EQ(colCount, 2); // id, name

    // В. Пропускаем описание схемы (нам оно сейчас не важно, важно число строк)
    for (size_t i = 0; i < colCount; ++i) {
        size_t nameLen = 0;
        std::memcpy(&nameLen, metaPtr, sizeof(size_t));
        metaPtr += sizeof(size_t);
        metaPtr += nameLen; // пропускаем имя
        metaPtr += sizeof(uint8_t); // пропускаем тип
    }

    // Г. Читаем batches_count
    size_t batchesCount = 0;
    std::memcpy(&batchesCount, metaPtr, sizeof(size_t));
    metaPtr += sizeof(size_t);

    // ПРОВЕРКА 1: Батчей должно быть больше 1, так как файл большой
    EXPECT_GT(batchesCount, 1) 
        << "For a 50KB file, we expect multiple batches (chunks), but got only " << batchesCount;

    // Д. Пропускаем массив Offsets (batchesCount * size_t)
    metaPtr += batchesCount * sizeof(size_t);

    // Е. Читаем массив Rows (batchesCount * size_t) и суммируем строки
    size_t total_rows_actual = 0;
    for (size_t i = 0; i < batchesCount; ++i) {
        size_t rowsInBatch = 0;
        std::memcpy(&rowsInBatch, metaPtr, sizeof(size_t));
        metaPtr += sizeof(size_t);
        
        total_rows_actual += rowsInBatch;
    }

    // ПРОВЕРКА 2: Сумма строк во всех батчах должна совпадать с тем, что мы записали
    EXPECT_EQ(total_rows_actual, total_rows_expected) 
        << "Data loss detected! Written rows vs BelZ metadata rows mismatch.";
}