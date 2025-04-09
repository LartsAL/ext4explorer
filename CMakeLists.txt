cmake_minimum_required(VERSION 3.30)
project(ext4explorer C)

set(CMAKE_C_STANDARD 17)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.c")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.h")

include_directories("include/")

add_executable(ext4explorer ${SOURCES} ${HEADERS})
