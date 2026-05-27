function(add_clang_format_targets TARGET_NAME SOURCE_ROOT)
    find_program(CLANG_FORMAT_EXE NAMES clang-format)

    if(NOT CLANG_FORMAT_EXE)
        message(STATUS "clang-format was not found; ${TARGET_NAME}-format targets will not be added")
        return()
    endif()

    file(GLOB_RECURSE CLANG_FORMAT_SOURCES CONFIGURE_DEPENDS
        "${SOURCE_ROOT}/*.cpp"
        "${SOURCE_ROOT}/*.h"
        "${SOURCE_ROOT}/*.hpp"
        "${SOURCE_ROOT}/*.cc"
        "${SOURCE_ROOT}/*.cxx"
    )

    list(FILTER CLANG_FORMAT_SOURCES EXCLUDE REGEX "/\\.cache/")
    list(FILTER CLANG_FORMAT_SOURCES EXCLUDE REGEX "/build/")
    list(FILTER CLANG_FORMAT_SOURCES EXCLUDE REGEX "/CMakeFiles/")

    if(NOT CLANG_FORMAT_SOURCES)
        message(STATUS "No C++ sources found for clang-format under ${SOURCE_ROOT}")
        return()
    endif()

    add_custom_target(${TARGET_NAME}-format
        COMMAND ${CLANG_FORMAT_EXE} -i ${CLANG_FORMAT_SOURCES}
        COMMENT "Formatting C++ sources under ${SOURCE_ROOT}"
        VERBATIM
    )

    add_custom_target(${TARGET_NAME}-format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${CLANG_FORMAT_SOURCES}
        COMMENT "Checking C++ formatting under ${SOURCE_ROOT}"
        VERBATIM
    )
endfunction()
