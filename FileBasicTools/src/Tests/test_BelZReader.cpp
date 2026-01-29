#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

#include "BelZReader.h"
#include "BelZWriter.h"

namespace fs = std::filesystem;

class BelZReaderTest : public ::testing::Test {
protected:
    std::string testFileCSV = "test_reader_data.csv";
    std::string testFileBelZ = "test_reader_data.belZ";

    void TearDown() override {
        // Удаляем файлы после тестов
        if (fs::exists(testFileCSV)) fs::remove(testFileCSV);
        if (fs::exists(testFileBelZ)) fs::remove(testFileBelZ);
    }

    // Хелпер: Генерирует .belZ файл с N батчами
    // Имитирует работу CSVConvertor (пишет по колонкам)
    void GenerateTestFile(size_t num_batches, size_t rows_per_batch) {
        // Чтобы BelZWriter создал файл, ему нужен путь к CSV (хотя бы фиктивный)
        std::ofstream(testFileCSV).close(); 
        
        BelZWriter writer(testFileCSV);
        MetaData meta;
        
        // Настраиваем схему: id (Int64), name (String)
        Scheme scheme;
        scheme.Push_Back({"id", ColumnType::Int64});
        scheme.Push_Back({"name", ColumnType::String});

        // Генерируем данные
        for (size_t b = 0; b < num_batches; ++b) {
            // 1. Запоминаем смещение начала батча
            meta.AddOffset(writer.GetOffSet());
            meta.AddRows(rows_per_batch);
            meta.AddCodec(0); // Заглушка, если есть

            // 2. Пишем КОЛОНКУ 1 (id) целиком для этого батча
            for (size_t r = 0; r < rows_per_batch; ++r) {
                int64_t val = (b * rows_per_batch) + r; // id = 0, 1, 2...
                writer.WriteData(std::to_string(val), ColumnType::Int64);
            }

            // 3. Пишем КОЛОНКУ 2 (name) целиком для этого батча
            for (size_t r = 0; r < rows_per_batch; ++r) {
                std::string val = "name_" + std::to_string((b * rows_per_batch) + r);
                writer.WriteData(val, ColumnType::String);
            }
        }

        // 4. Пишем метаданные в конец файла
        meta.SetScheme(std::move(scheme));
        writer.WriteMeta(meta);
    }
};

// 1. Тест чтения одного батча
TEST_F(BelZReaderTest, ReadSingleBatch) {
    size_t rows = 3;
    GenerateTestFile(1, rows); // 1 батч, 3 строки
    BelZReader reader(testFileBelZ);
    
    ASSERT_FALSE(reader.Empty());

    Batch batch;
    reader.ReadBatch(batch);

    // Проверки структуры батча
    ASSERT_EQ(batch.Size(), 2); // 2 колонки
    
    // --- Проверка колонки Int64 ---
    auto col1 = batch.GetColumn(0);
    ASSERT_TRUE(Is<Int64Column>(col1));
    auto intCol = As<Int64Column>(col1);
    ASSERT_EQ(intCol->Size(), rows);
    EXPECT_EQ((*intCol)[0], 0);
    EXPECT_EQ((*intCol)[1], 1);
    EXPECT_EQ((*intCol)[2], 2);

    // --- Проверка колонки String ---
    auto col2 = batch.GetColumn(1);
    ASSERT_TRUE(Is<StringColumn>(col2));
    auto strCol = As<StringColumn>(col2);
    ASSERT_EQ(strCol->Size(), rows);
    EXPECT_EQ((*strCol)[0], "name_0");
    EXPECT_EQ((*strCol)[1], "name_1");
    EXPECT_EQ((*strCol)[2], "name_2");

    // Теперь ридер должен быть пуст
    EXPECT_TRUE(reader.Empty());
}

// 2. Тест чтения нескольких батчей (последовательное чтение)
TEST_F(BelZReaderTest, ReadMultipleBatches) {
    size_t batches = 2;
    size_t rows = 2;
    GenerateTestFile(batches, rows); 
    // Batch 0: id=[0, 1], name=["name_0", "name_1"]
    // Batch 1: id=[2, 3], name=["name_2", "name_3"]

    BelZReader reader(testFileBelZ);
    
    // --- Читаем 1-й батч ---
    ASSERT_FALSE(reader.Empty());
    Batch batch1;
    reader.ReadBatch(batch1);
    
    auto col_b1 = As<Int64Column>(batch1.GetColumn(0));
    EXPECT_EQ((*col_b1)[0], 0); // Проверка данных
    EXPECT_EQ((*col_b1)[1], 1);

    // --- Читаем 2-й батч ---
    ASSERT_FALSE(reader.Empty());
    Batch batch2;
    reader.ReadBatch(batch2);
    
    auto col_b2 = As<Int64Column>(batch2.GetColumn(0));
    EXPECT_EQ((*col_b2)[0], 2); // Проверка данных (следующие ID)
    EXPECT_EQ((*col_b2)[1], 3);

    // --- Конец файла ---
    EXPECT_TRUE(reader.Empty());
}

// 3. Тест на проверку типов колонок (Scheme consistency)
TEST_F(BelZReaderTest, ColumnTypeConsistency) {
    GenerateTestFile(1, 1);
    BelZReader reader(testFileBelZ);
    Batch batch;
    reader.ReadBatch(batch);

    // Схема была: Int64, String
    EXPECT_EQ(batch.GetType(0), ColumnType::Int64);
    EXPECT_EQ(batch.GetType(1), ColumnType::String);
    
    // Проверим, что GetColumn возвращает правильные указатели
    EXPECT_NE(As<Int64Column>(batch.GetColumn(0)), nullptr);
    EXPECT_NE(As<StringColumn>(batch.GetColumn(1)), nullptr);
    
    // Проверим, что каст в неправильный тип вернет nullptr
    EXPECT_EQ(As<StringColumn>(batch.GetColumn(0)), nullptr);
}

// 4. Тест чтения пустого файла (или файла с 0 батчей)
TEST_F(BelZReaderTest, EmptyData) {
    // Генерируем файл с 0 батчей
    GenerateTestFile(0, 0); 
    
    BelZReader reader(testFileBelZ);
    
    // Сразу должен быть пустым
    EXPECT_TRUE(reader.Empty());
    
    // Попытка прочитать батч не должна ронять программу (зависит от вашей реализации)
    // Обычно либо ничего не происходит, либо throw. 
    // Если у вас нет проверок внутри ReadBatch, этот тест можно не делать или обернуть в try/catch.
    Batch batch;
    // reader.ReadBatch(batch); // Раскомментировать, если поведение определено
}

// 5. Тест восстановления после перезагрузки (Persistency)
// Проверяем, что файл действительно читается с диска, а не берется из памяти writer-а
TEST_F(BelZReaderTest, Persistence) {
    {
        GenerateTestFile(1, 5);
        // Writer уничтожается здесь, файл закрывается
    }

    BelZReader reader(testFileBelZ);
    Batch batch;
    reader.ReadBatch(batch);
    
    ASSERT_EQ(batch.GetColumn(0)->Size(), 5);
}