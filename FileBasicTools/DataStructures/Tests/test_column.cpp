#include <gtest/gtest.h>
#include "Column.h" // Убедитесь, что путь верный
#include <memory>
#include <vector>

// ==========================================
// Тесты для Int64Column
// ==========================================

TEST(Int64ColumnTest, BasicOperations) {
    auto col = std::make_shared<Int64Column>();
    
    // Проверка Reserve (просто что не падает)
    EXPECT_NO_THROW(col->Reserve(100));
    
    // Добавление чисел напрямую
    col->Push_Back(42);
    col->Push_Back(-100);
    
    EXPECT_EQ(col->Size(), 2);
    EXPECT_EQ((*col)[0], 42);
    EXPECT_EQ((*col)[1], -100);
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
    StringColumn col;
    
    std::string s = "lvalue";
    col.Push_Back(s);           // lvalue version
    col.Push_Back("rvalue");    // rvalue version
    
    ASSERT_EQ(col.Size(), 2);
    EXPECT_EQ(col[0], "lvalue");
    EXPECT_EQ(col[1], "rvalue");
}

TEST(StringColumnTest, AppendFromString) {
    StringColumn col;
    col.AppendFromString("hello");
    col.AppendFromString("world");
    
    ASSERT_EQ(col.Size(), 2);
    EXPECT_EQ(col[0], "hello");
    EXPECT_EQ(col[1], "world");
}

TEST(StringColumnTest, OutOfBounds) {
    StringColumn col;
    col.Push_Back("test");
    
    EXPECT_THROW(col[1], std::runtime_error);
}

// ==========================================
// Тесты для Helper функций (Is / As)
// ==========================================

TEST(ColumnUtilsTest, IsCheck) {
    // Создаем Int64Column, но храним как базовый указатель
    std::shared_ptr<Column> intCol = std::make_shared<Int64Column>();
    std::shared_ptr<Column> strCol = std::make_shared<StringColumn>();
    
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
    // Проверка работы через виртуальные методы
    std::vector<std::shared_ptr<Column>> columns;
    
    columns.push_back(std::make_shared<Int64Column>());
    columns.push_back(std::make_shared<StringColumn>());
    
    // Вызываем виртуальный AppendFromString
    columns[0]->AppendFromString("100");
    columns[1]->AppendFromString("text");
    
    // Проверяем результат через каст (так как оператор [] не виртуальный)
    EXPECT_EQ((*As<Int64Column>(columns[0]))[0], 100);
    EXPECT_EQ((*As<StringColumn>(columns[1]))[0], "text");
}