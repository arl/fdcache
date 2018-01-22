cmake_minimum_required(VERSION 2.8)

project(fdcache C)
add_executable(${PROJECT_NAME}
	"main.c"
	"fdcache.c"
	"fdcache.h"
	"bitmap.c"
	"bitmap.h"
)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(GLib)

if(GLib_FOUND)
   include_directories(${GLib_INCLUDE_DIRS})
   target_link_libraries(${PROJECT_NAME} ${GLib_LIBRARY})
endif(GLib_FOUND)

add_subdirectory(tests)
#link_directories("/usr/local/lib/")
target_link_libraries(${PROJECT_NAME} "/usr/local/lib/libjemalloc.so")
