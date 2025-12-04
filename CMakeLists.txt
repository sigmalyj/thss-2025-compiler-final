cmake_minimum_required(VERSION 3.10)
project(compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
option(ENABLE_UNIT_TESTS "Build unit tests" OFF)

# Include antlr4 runtime
add_subdirectory(third_party/antlr4-runtime)

# Headers for include
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/antlr4-runtime/runtime/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/antlr
)

# Generate executable files
file(GLOB SRC_FILES
  src/*.cpp
  src/antlr/*.cpp
)

add_executable(compiler ${SRC_FILES})

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