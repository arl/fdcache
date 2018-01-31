cmake_minimum_required(VERSION 2.8)

project(fdcache C)
add_executable(${PROJECT_NAME}
    "fdcache_internal.h"
    "fdcache.h"
    "fdcache.c"
    "roaring.h"
    "roaring.c"
    "main.c"
)


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(GLib REQUIRED)
if(GLib_FOUND)
   include_directories(${GLib_INCLUDE_DIRS})
   target_link_libraries(${PROJECT_NAME} ${GLib_LIBRARY})
endif(GLib_FOUND)

find_package(JeMalloc REQUIRED)
if(JEMALLOC_FOUND)
   include_directories(${JEMALLOC_INCLUDE_DIR})
   target_link_libraries(${PROJECT_NAME} ${JEMALLOC_LIBRARY})
endif(JEMALLOC_FOUND)

add_subdirectory(tests)
