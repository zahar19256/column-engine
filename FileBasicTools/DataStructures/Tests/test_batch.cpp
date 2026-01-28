#include <gtest/gtest.h>
#include "Batch.h"
#include "Column.h" // Здесь лежат Int64Column, StringColumn, As<T>
#include "Scheme.h"
#include "Row.h"
#include <string>
#include <vector>
#include <memory>

// Вспомогательная функция для создания тестового чанка
std::vector<Row<std::string>> CreateMockChunk() {
    std::vector<Row<std::string>> chunk;
    
    // Строка 1: "100", "Alice"
    Row<std::string> row1;
    row1.Add("100");
    row1.Add("Alice");
    chunk.push_back(row1);

    // Строка 2: "200", "Bob"
    Row<std::string> row2;
    row2.Add("200");
    row2.Add("Bob");
    chunk.push_back(row2);

    return chunk;
}

// 1. Тест пустого батча
TEST(BatchTest, DefaultConstructor) {
    Batch batch;
    EXPECT_TRUE(batch.Empty());
    EXPECT_EQ(batch.Size(), 0);
}

// 2. Тест ручной сборки батча (AddColumn)
TEST(BatchTest, ManualAssembly) {
    Batch batch;
    
    // Создаем схему для порядка
    Scheme scheme;
    scheme.Push_Back({"num", ColumnType::Int64});
    batch.SetScheme(scheme);

    // Создаем и заполняем колонку
    auto intCol = std::make_shared<Int64Column>();
    intCol->Push_Back(42);
    
    // Добавляем в батч
    batch.AddColumn(intCol);

    ASSERT_EQ(batch.Size(), 1); // 1 колонка
    EXPECT_FALSE(batch.Empty());
    
    // Проверяем, что достаем то же самое
    auto retrieved = batch.GetColumn(0);
    EXPECT_EQ(retrieved, intCol);
    
    // Проверяем данные внутри
    auto casted = As<Int64Column>(retrieved);
    ASSERT_NE(casted, nullptr);
    EXPECT_EQ((*casted)[0], 42);
}

// 3. ГЛАВНЫЙ ТЕСТ: Создание батча из строк (ChunkToBatch)
TEST(BatchTest, ConstructorFromChunk_MixedTypes) {
    // 1. Определяем схему: Int64, String
    Scheme scheme;
    scheme.Push_Back({"id", ColumnType::Int64});
    scheme.Push_Back({"name", ColumnType::String});

    // 2. Берем тестовые данные (100, Alice), (200, Bob)
    auto chunk = CreateMockChunk();

    // 3. Создаем Batch. Тут должна сработать магия парсинга.
    Batch batch(chunk, scheme);

    // 4. Проверки структуры
    ASSERT_EQ(batch.Size(), 2); // Должно быть 2 колонки
    EXPECT_EQ(batch.GetType(0), ColumnType::Int64);
    EXPECT_EQ(batch.GetType(1), ColumnType::String);

    // 5. Проверка первой колонки (Int64)
    auto col0 = batch.GetColumn(0);
    ASSERT_NE(col0, nullptr);
    
    // Используем As<T> из Column.h
    auto intCol = As<Int64Column>(col0);
    ASSERT_NE(intCol, nullptr) << "Column 0 should be Int64Column";
    ASSERT_EQ(intCol->Size(), 2);
    EXPECT_EQ((*intCol)[0], 100);
    EXPECT_EQ((*intCol)[1], 200);

    // 6. Проверка второй колонки (String)
    auto col1 = batch.GetColumn(1);
    auto strCol = As<StringColumn>(col1);
    ASSERT_NE(strCol, nullptr) << "Column 1 should be StringColumn";
    ASSERT_EQ(strCol->Size(), 2);
    EXPECT_EQ((*strCol)[0], "Alice");
    EXPECT_EQ((*strCol)[1], "Bob");
}

// 4. Тест парсинга некорректных данных
TEST(BatchTest, ConstructorFromChunk_InvalidData) {
    Scheme scheme;
    scheme.Push_Back({"must_be_int", ColumnType::Int64});

    Row<std::string> badRow;
    badRow.Add("NOT_A_NUMBER"); // Это вызовет ошибку в Int64Column::AppendFromString

    std::vector<Row<std::string>> chunk;
    chunk.push_back(badRow);

    // Ожидаем, что Batch не проглотит исключение, а выбросит его дальше
    // std::stoll кидает invalid_argument
    EXPECT_THROW(Batch(chunk, scheme), std::invalid_argument);
}

// 5. Тест очистки
TEST(BatchTest, Clear) {
    Scheme scheme;
    scheme.Push_Back({"col", ColumnType::Int64});
    
    std::vector<Row<std::string>> chunk;
    Row<std::string> row; 
    row.Add("1");
    chunk.push_back(row);

    Batch batch(chunk, scheme);
    ASSERT_EQ(batch.Size(), 1);

    batch.Clear();

    EXPECT_EQ(batch.Size(), 0);
    EXPECT_TRUE(batch.Empty());
}

// 6. Тест геттеров с выходом за границы
TEST(BatchTest, OutOfBoundsAccess) {
    Batch batch;
    // Если в вашей реализации используется vector::at(), будет throw.
    // Если operator[], то тест писать опасно (UB).
    // Предположим безопасный доступ или проверим, что вызовет исключение вектора:
    
    // Раскомментируйте, если есть проверки границ:
    // EXPECT_THROW(batch.GetColumn(0), std::out_of_range);
    // EXPECT_THROW(batch.GetType(0), std::out_of_range);
}