cmake_minimum_required (VERSION 2.8.12)
PROJECT(rdbfw)

macro(use_c99)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "-std=gnu99 ${CMAKE_C_FLAGS}")
    endif ()
  else ()
    set (CMAKE_C_STANDARD 99)
  endif ()
endmacro(use_c99)

macro(use_c11)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "-std=gnu11 ${CMAKE_C_FLAGS}")
    endif ()
  else ()
    set (CMAKE_C_STANDARD 11)
    set (CMAKE_CXX_STANDARD 11)
  endif ()
endmacro(use_c11)

macro(use_c14)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "-std=gnu14 ${CMAKE_C_FLAGS}")
    endif ()
  else ()
    set (CMAKE_C_STANDARD 11)
    set (CMAKE_CXX_STANDARD 14)
  endif ()
endmacro(use_c14)

use_c14()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -fms-extensions")
set(CMAKE_C_FLAGS_DEBUG "-Wall -O0 -g")
set(CMAKE_C_FLAGS_RELEASE "-Wall -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fPIC -fms-extensions")
set(CMAKE_CXX_FLAGS_DEBUG "-Wall -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-Wall -O3")

add_definitions ( -DBUILDING_LIB )

include_directories("${PROJECT_SOURCE_DIR}/include")

set (HEADERS
    ${HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR/include}
    "${PROJECT_SOURCE_DIR}/include"
    )#PARENT_SCOPE)

link_directories(
)


execute_process(
    COMMAND git describe --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )

configure_file (
    "${PROJECT_SOURCE_DIR}/include/version.h.in"
    "${PROJECT_BINARY_DIR}/version.h"
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g -export-dynamic")

message ("libdir==${CMAKE_INSTALL_LIBDIR}")
message ("libdir==${CMAKE_INSTALL_PREFIX}")

add_subdirectory(c)
add_subdirectory(test)

enable_testing()

