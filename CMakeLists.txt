cmake_minimum_required(VERSION 3.22)
project(column-engine)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic -Wno-unused-parameter -Wno-unused-function")

include(cmake/ClangFormat.cmake)
add_clang_format_targets(project "${CMAKE_CURRENT_SOURCE_DIR}")
add_custom_target(format DEPENDS project-format)
add_custom_target(format-check DEPENDS project-format-check)

add_executable(main main.cpp)
