cmake_minimum_required(VERSION 3.22)
project(FileBasicTools LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic -Wno-unused-parameter -Wno-unused-function"
)

option(FILE_BASIC_TOOLS_BUILD_TESTS "Build FileBasicTools tests" ${PROJECT_IS_TOP_LEVEL})

set(DATASTRUCT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/Column.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/Row.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/Batch.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/Batch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/Scheme.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/Scheme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/MetaData.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/MetaData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/GermanString.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/GermanString.cpp
)

find_package(absl REQUIRED)

add_library(data_structures ${DATASTRUCT_SOURCES})
target_include_directories(data_structures
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures
)
target_compile_features(data_structures PUBLIC cxx_std_20)

add_library(file_tools
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CSVReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CSVReader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CSVWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CSVWriter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BelZReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BelZReader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BelZWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BelZWriter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Coder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Coder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CSVConvertor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CSVConvertor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BelZConvertor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BelZConvertor.h
)

target_include_directories(file_tools
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(file_tools
    PUBLIC
        data_structures
        absl::flat_hash_map
        absl::hash
)

if(FILE_BASIC_TOOLS_BUILD_TESTS)
    include(CTest)
    enable_testing()

    find_package(GTest REQUIRED)

    add_executable(test_data_structures
        #${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/Tests/test_row.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/Tests/test_scheme.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/Tests/test_metadata.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/Tests/test_column.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/Tests/test_batch.cpp
    )

    add_executable(test_src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/test_CSVReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/test_CSVWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/test_BelZReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/test_BelZWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/test_CSVConvertor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/test_BelZConvertor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/test_BelZConvertorAdvanced.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/test_HitsRoundTrip.cpp
    )

    add_executable(test_bench
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/bench.cpp
    )

    add_executable(test_scan
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/scan_bench.cpp
    )

    target_compile_options(test_bench PRIVATE -O3 -march=native -mtune=native)
    target_compile_options(test_scan PRIVATE -O3 -march=native -mtune=native)

    target_link_libraries(test_data_structures
        PRIVATE
            data_structures
            GTest::gtest
            GTest::gtest_main
    )

    target_link_libraries(test_src
        PRIVATE
            data_structures
            file_tools
            GTest::gtest
            GTest::gtest_main
    )

    target_link_libraries(test_bench
        PRIVATE
            data_structures
            file_tools
            GTest::gtest
            GTest::gtest_main
            absl::flat_hash_map
            absl::hash
    )

    target_link_libraries(test_scan
        PRIVATE
            data_structures
            file_tools
            GTest::gtest
            GTest::gtest_main
    )

    add_test(NAME test_src COMMAND test_src)
    add_test(NAME test_bench COMMAND test_bench)
    add_test(NAME test_scan COMMAND test_scan)
    add_test(NAME test_data_structures COMMAND test_data_structures)
endif()
