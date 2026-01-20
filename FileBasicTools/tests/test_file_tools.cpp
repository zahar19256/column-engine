#include <gtest/gtest.h>

#include "file_tools.h"

TEST(FilesExists, EasyTest) {
    EXPECT_EQ(FileExists("/home/zahar/Документы/colomn-engine/column-engine/FileBasicTools/tests/1.csv") , 1);
    EXPECT_EQ(FileExists("/home/zahar/Документы/colomn-engine/column-engine/FileBasicTools/tests/100.csv") , 0);
}

TEST(ReadFile, DATA_and_SIZE) {
    EXPECT_EQ(FileExists("/home/zahar/Документы/colomn-engine/column-engine/FileBasicTools/tests/1.csv") , 1);

    std::string standart_path = "/home/zahar/Документы/colomn-engine/column-engine/FileBasicTools/tests/";
    std::vector<uint8_t> result = ReadFileData(standart_path + "0.csv");
    std::vector<uint8_t> correct_data;

    EXPECT_EQ(result.size(), 11);

    result = ReadFileData(standart_path + "1.csv");
    correct_data = {0x31, 0x2c, 0x32, 0x2c, 0x66, 0x69, 0x72, 0x73, 
	0x74, 0x2c, 0x34, 0x0a, 0x35, 0x2c, 0x31, 0x2c, 
	0x73, 0x65, 0x63, 0x6f, 0x6e, 0x64, 0x2c, 0x32, 
	0x0a, 0x38, 0x2c, 0x31, 0x37, 0x2c, 0x74, 0x68, 
	0x69, 0x72, 0x64, 0x2c, 0x32};
    EXPECT_EQ(result, correct_data);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}