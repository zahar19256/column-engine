cmake_minimum_required(VERSION 3.22)

project(Engine LANGUAGES CXX)

include(CTest)
enable_testing()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/ClangFormat.cmake")
add_clang_format_targets(engine "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET engine-format AND NOT TARGET format)
    add_custom_target(format DEPENDS engine-format)
endif()
if(TARGET engine-format-check AND NOT TARGET format-check)
    add_custom_target(format-check DEPENDS engine-format-check)
endif()

add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../FileBasicTools"
    "${CMAKE_CURRENT_BINARY_DIR}/FileBasicTools"
)

find_package(re2 CONFIG REQUIRED)

add_library(Engine
    Execution/Executor.cpp
    Execution/Executor.h
    Functions/Filter.h
    Functions/Aggregation.h
    Functions/Expression.h
    Functions/TopK.cpp
    Functions/TopK.h
    Helpers/AggrBuilder.cpp
    Helpers/AggrBuilder.h
    Helpers/ExprHelper.h
    Helpers/Helpers.h
    Helpers/PlanBuilder.h
    Operators/ScalarOperators.cpp
    Operators/ScalarOperators.h
    Planer/Base.h
)

target_include_directories(Engine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(Engine PUBLIC cxx_std_20)

target_compile_options(Engine
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-function
)

target_link_libraries(Engine
    PUBLIC
        file_tools
        re2::re2
)

add_custom_target(Executor DEPENDS Engine)

find_package(GTest REQUIRED)

add_executable(test_engine
    Tests/test_engine.cpp
)

add_executable(clickbench_q1
    ClickBenchQ1.cpp
)

add_executable(clickbench_runner
    ClickBenchRunner.cpp
)

add_executable(clickbench_convert
    ClickBenchConvert.cpp
)

target_link_libraries(test_engine
    PRIVATE
        Engine
        GTest::gtest
        GTest::gtest_main
)

target_link_libraries(clickbench_q1
    PRIVATE
        Engine
)

target_link_libraries(clickbench_runner
    PRIVATE
        Engine
)

target_link_libraries(clickbench_convert
    PRIVATE
        Engine
)

add_test(NAME test_engine COMMAND test_engine)
