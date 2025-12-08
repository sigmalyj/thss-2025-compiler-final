cmake_minimum_required(VERSION 3.10)
project(compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
option(ENABLE_UNIT_TESTS "Build unit tests" OFF)

set(ANTLR_JAR ${CMAKE_CURRENT_SOURCE_DIR}/lib/antlr-4.13.1-complete.jar)
set(ANTLR_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/antlr)

file(MAKE_DIRECTORY ${ANTLR_GEN_DIR})

set(ANTLR_GENERATED_SRC
    ${ANTLR_GEN_DIR}/SysYLexer.cpp
    ${ANTLR_GEN_DIR}/SysYParser.cpp
    ${ANTLR_GEN_DIR}/SysYParserBaseVisitor.cpp
    ${ANTLR_GEN_DIR}/SysYParserVisitor.cpp
)

set(ANTLR_GENERATED_HEADERS
    ${ANTLR_GEN_DIR}/SysYLexer.h
    ${ANTLR_GEN_DIR}/SysYParser.h
    ${ANTLR_GEN_DIR}/SysYParserBaseVisitor.h
    ${ANTLR_GEN_DIR}/SysYParserVisitor.h
    ${ANTLR_GEN_DIR}/SysYLexer.tokens
    ${ANTLR_GEN_DIR}/SysYParser.tokens
)

add_custom_command(
    OUTPUT ${ANTLR_GENERATED_SRC} ${ANTLR_GENERATED_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E echo "Generating ANTLR parser"
    COMMAND java -jar ${ANTLR_JAR} -Dlanguage=Cpp -visitor -no-listener -o ${ANTLR_GEN_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/SysYLexer.g4 ${CMAKE_CURRENT_SOURCE_DIR}/SysYParser.g4
    DEPENDS SysYLexer.g4 SysYParser.g4 ${ANTLR_JAR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running ANTLR4 to generate SysY parser"
)

add_custom_target(SysYParserGen DEPENDS ${ANTLR_GENERATED_SRC} ${ANTLR_GENERATED_HEADERS})

# Include antlr4 runtime
add_subdirectory(third_party/antlr4-runtime)

# Headers for include
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/antlr4-runtime/runtime/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/antlr
)

# Generate executable files
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# Exclude generated files from GLOB to avoid duplication if they are already there
list(FILTER SRC_FILES EXCLUDE REGEX ".*/src/antlr/.*")

list(APPEND SRC_FILES ${ANTLR_GENERATED_SRC})

add_executable(compiler ${SRC_FILES})
add_dependencies(compiler SysYParserGen)

# Link runtime
target_link_libraries(compiler antlr4_shared)

if(ENABLE_UNIT_TESTS)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/03597a01ee50ed33e9fd716149c63099179ad6c5.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/test/unit/*.cpp
    )

    if(UNIT_TEST_SOURCES)
        enable_testing()
        add_executable(unit_tests ${UNIT_TEST_SOURCES})
        target_link_libraries(unit_tests gtest_main antlr4_shared)
        target_include_directories(unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        include(GoogleTest)
        gtest_discover_tests(unit_tests)
    else()
        message(WARNING "ENABLE_UNIT_TESTS is ON but no sources were found in test/unit")
    endif()
endif()

# Config ASAN
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

if(ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/fsanitize=address)
    endif()
endif()