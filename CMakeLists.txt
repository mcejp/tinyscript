#set(CMAKE_VERBOSE_MAKEFILE ON)
cmake_minimum_required(VERSION 3.1)
project(tinyscript)
set(CMAKE_CXX_STANDARD 14)

set(HEADER_FILES
  include/tinyapi.h
  include/tsval.h

  src/parse.h
  )

set(SOURCE_FILES
  src/parse.c
  src/tinyscript.c
  src/tsval.c

  dependencies/tokenfactory/ast.c
  dependencies/tokenfactory/tokenbuffer.c
  dependencies/tokenfactory/tokenfactory.c
  )

add_executable(tsi ${SOURCE_FILES} ${HEADER_FILES})

target_compile_options(tsi PUBLIC "-fsanitize=address")
target_link_libraries(tsi -fsanitize=address)

target_include_directories(tsi PUBLIC include)
target_include_directories(tsi PRIVATE dependencies/parse_args dependencies/tokenfactory)
