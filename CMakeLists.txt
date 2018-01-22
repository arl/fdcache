cmake_minimum_required(VERSION 2.8)

project(fdcache C)
add_executable(${PROJECT_NAME}
	"fdcache.c"
	"fdcache.h"
	"bitmap.c"
	"bitmap.h"
        "main.c"
)


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
