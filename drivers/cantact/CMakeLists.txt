cmake_minimum_required(VERSION 3.20)
project(twocan_cantact_driver LANGUAGES CXX)

enable_testing()

# Modern Windows CANable/CANtact SLCAN driver for TwoCan.
add_library(cantact SHARED
    src/cantact_driver.cpp
    src/slcan_parser.cpp
    include/cantact_driver.h
    include/slcan_parser.h
)

target_include_directories(cantact PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(cantact PRIVATE cxx_std_17)
target_compile_definitions(cantact PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(cantact PRIVATE setupapi advapi32)
set_target_properties(cantact PROPERTIES
    PREFIX ""
    OUTPUT_NAME "cantact"
)

add_executable(cantact_parser_tests
    tests/slcan_parser_tests.cpp
    src/slcan_parser.cpp
)
target_include_directories(cantact_parser_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(cantact_parser_tests PRIVATE cxx_std_17)
target_compile_definitions(cantact_parser_tests PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

add_test(NAME cantact_slcan_parser COMMAND cantact_parser_tests)
