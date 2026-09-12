cmake_minimum_required(VERSION 4.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


project(squirrel)

file(GLOB_RECURSE LIB_SRCS "squirrel/**")
add_library(squirrel STATIC ${LIB_SRCS})
target_include_directories(squirrel PUBLIC "squirrel/")

file(GLOB_RECURSE SRCS "runner/**")
add_executable(squirrelrun ${SRCS})
target_link_libraries(squirrelrun PUBLIC squirrel)