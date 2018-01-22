cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0017 NEW)

find_package(CUnit REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -g")
include_directories("${CMAKE_SOURCE_DIR}/include")
message(STATUS "cmake link dir from tests: ${CMAKE_BINARY_DIR}/lib")
link_directories("${CMAKE_BINARY_DIR}/lib")

add_executable(bitmap_test bitmap_test.c ../bitmap.c ../bitmap.h)
target_link_libraries(bitmap_test ${CUNIT_LIBRARIES})