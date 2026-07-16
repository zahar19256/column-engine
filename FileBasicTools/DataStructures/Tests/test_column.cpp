#include "Column.h" // Убедитесь, что путь верный
#include <gtest/gtest.h>
#include <memory>
#include <vector>

// ==========================================
// Тесты для Int64Column
// ==========================================

TEST(Int64ColumnTest, BasicOperations) {
    auto col = std::make_shared<Int64Column>();

    // Проверка Reserve (просто что не падает)
    EXPECT_NO_THROW(col->Reserve(100));
    EXPECT_EQ(col->Size(), 0);
    col->Resize(3);
    EXPECT_EQ(col->Size(), 3);

    // Добавление чисел напрямую
    col->Push_Back(42);
    col->Push_Back(-100);

    EXPECT_EQ(col->Size(), 5);
    EXPECT_EQ((*col)[3], 42);
    EXPECT_EQ((*col)[4], -100);
}

TEST(Int64ColumnTest, AppendFromString) {
    Int64Column col;

    // Нормальные числа
    col.AppendFromString("123");
    col.AppendFromString("-555");

    ASSERT_EQ(col.Size(), 2);
    EXPECT_EQ(col[0], 123);
    EXPECT_EQ(col[1], -555);
}

TEST(Int64ColumnTest, AppendFromString_InvalidData) {
    Int64Column col;

    // std::stoll бросает std::invalid_argument, если строка не число
    EXPECT_THROW(col.AppendFromString("abc"), std::invalid_argument);

    // std::stoll бросает std::out_of_range, если число слишком большое для int64
    // 9223372036854775807 - макс int64. Добавим еще цифр.
    EXPECT_THROW(col.AppendFromString("999999999999999999999"), std::out_of_range);
}

TEST(Int64ColumnTest, OutOfBounds) {
    Int64Column col;
    col.Push_Back(10);

    // Индекс в пределах
    EXPECT_NO_THROW(col[0]);

    // Индекс за пределами (ваша реализация бросает runtime_error)
    EXPECT_THROW(col[1], std::runtime_error);
    EXPECT_THROW(col[100], std::runtime_error);
}

TEST(Int64ColumnTest, RawPointerAccess) {
    Int64Column col;
    col.Push_Back(10);
    col.Push_Back(20);
    col.Push_Back(30);

    const int64_t* ptr = col.Data();
    ASSERT_NE(ptr, nullptr);

    // Проверяем арифметику указателей (так как это vector, память непрерывна)
    EXPECT_EQ(ptr[0], 10);
    EXPECT_EQ(ptr[1], 20);
    EXPECT_EQ(ptr[2], 30);
}

// ==========================================
// Тесты для StringColumn
// ==========================================

TEST(StringColumnTest, BasicOperations) {
    Utility::StringArena arena;
    StringColumn col(&arena);
    col.Reserve(100);
    EXPECT_EQ(col.Size(), 0);

    std::string s = "lvalue";
    col.AppendFromString(s.data(), s.size());
    col.AppendFromString("rvalue", 6);

    ASSERT_EQ(col.Size(), 2);
    EXPECT_EQ(col.At_view(0), "lvalue");
    EXPECT_EQ(col.At_view(1), "rvalue");
}

TEST(StringColumnTest, AppendLongStringUsesArena) {
    Utility::StringArena arena;
    StringColumn col(&arena);

    std::string value = "long-string-value";
    col.AppendFromString(value.data(), value.size());

    ASSERT_EQ(col.Size(), 1);
    EXPECT_EQ(col.At_view(0), value);
    EXPECT_GT(arena.MemoryUsed(), 0);
}

TEST(StringColumnTest, AppendFromString) {
    Utility::StringArena arena;
    StringColumn col(&arena);
    col.AppendFromString("hello", 5);
    col.AppendFromString("world", 5);

    ASSERT_EQ(col.Size(), 2);
    EXPECT_EQ(col.At_view(0), "hello");
    EXPECT_EQ(col.At_view(1), "world");
}

TEST(StringColumnTest, OutOfBounds) {
    Utility::StringArena arena;
    StringColumn col(&arena);
    col.AppendFromString("test", 4);

    EXPECT_THROW(col[1], std::runtime_error);
}

// ==========================================
// Тесты для Helper функций (Is / As)
// ==========================================

TEST(ColumnUtilsTest, IsCheck) {
    Utility::StringArena arena;
    // Создаем Int64Column, но храним как базовый указатель
    std::shared_ptr<Column> intCol = std::make_shared<Int64Column>();
    std::shared_ptr<Column> strCol = std::make_shared<StringColumn>(&arena);

    // Проверяем IntCol
    EXPECT_TRUE(Is<Int64Column>(intCol));
    EXPECT_FALSE(Is<StringColumn>(intCol));

    // Проверяем StrCol
    EXPECT_TRUE(Is<StringColumn>(strCol));
    EXPECT_FALSE(Is<Int64Column>(strCol));
}

TEST(ColumnUtilsTest, AsCast) {
    std::shared_ptr<Column> basePtr = std::make_shared<Int64Column>();

    // Успешный каст
    std::shared_ptr<Int64Column> concretePtr = As<Int64Column>(basePtr);
    ASSERT_NE(concretePtr, nullptr);

    // Теперь можно вызывать методы конкретного класса
    concretePtr->Push_Back(123);
    EXPECT_EQ(concretePtr->Size(), 1);

    // Неудачный каст (должен вернуть nullptr)
    std::shared_ptr<StringColumn> wrongPtr = As<StringColumn>(basePtr);
    EXPECT_EQ(wrongPtr, nullptr);
}

TEST(ColumnUtilsTest, Polymorphism) {
    Utility::StringArena arena;
    // Проверка работы через виртуальные методы
    std::vector<std::shared_ptr<Column>> columns;

    columns.push_back(std::make_shared<Int64Column>());
    columns.push_back(std::make_shared<StringColumn>(&arena));

    // Вызываем виртуальный AppendFromString
    std::string s1 = "100";
    std::string s2 = "text";
    columns[0]->AppendFromString(s1.data(), 3);
    columns[1]->AppendFromString(s2.data(), 4);

    // Проверяем результат через каст (так как оператор [] не виртуальный)
    EXPECT_EQ((*As<Int64Column>(columns[0]))[0], 100);
    EXPECT_EQ(As<StringColumn>(columns[1])->At_view(0), "text");
}

TEST(ColumnUtilsTest, GetScalarValue) {
    Utility::StringArena arena;
    std::shared_ptr<Column> int16_col = std::make_shared<Int16Column>();
    std::shared_ptr<Column> string_col = std::make_shared<StringColumn>(&arena);
    std::shared_ptr<Column> double_col = std::make_shared<DoubleColumn>();
    std::shared_ptr<Column> int128_col = std::make_shared<Int128Column>();

    As<Int16Column>(int16_col)->Push_Back(42);
    As<StringColumn>(string_col)->AppendFromString("key", 3);
    As<DoubleColumn>(double_col)->Push_Back(3.5);
    As<Int128Column>(int128_col)->Push_Back(static_cast<__int128_t>(1) << 80);

    EXPECT_EQ(std::get<int64_t>(int16_col->GetScalarValue(0)), 42);
    EXPECT_EQ(std::get<GermanStr>(string_col->GetScalarValue(0)).View(), "key");
    EXPECT_DOUBLE_EQ(std::get<double>(double_col->GetScalarValue(0)), 3.5);
    EXPECT_EQ(std::get<__int128_t>(int128_col->GetScalarValue(0)), static_cast<__int128_t>(1) << 80);
}
