cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0017 NEW)

find_package(CUnit REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -g")
message(STATUS "cmake link dir from tests: ${CMAKE_BINARY_DIR}/lib")

SET(fdcache_test_SRCS
   test_helpers.h
   test_helpers.c
   fdcache_test.c
   ../fdcache.c
)
add_executable(fdcache_test ${fdcache_test_SRCS})
target_link_libraries(fdcache_test ${CUNIT_LIBRARIES}
                                   ${JEMALLOC_LIBRARY}
                                   ${GLib_LIBRARY}
                                   roaring)
