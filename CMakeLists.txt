cmake_minimum_required(VERSION 3.28)

project(PATM
    VERSION 0.0.0
    DESCRIPTION "PATM - Pipeline Automation Tool Manager"
    LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(VERSION_STAGE "experimental patch 00")
configure_file(${CMAKE_SOURCE_DIR}/src/core/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Werror -fno-strict-aliasing)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql)
pkg_check_modules(LIBPQ REQUIRED IMPORTED_TARGET libpq)
pkg_check_modules(MARIADB REQUIRED IMPORTED_TARGET libmariadb)
pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
find_package(Python3 REQUIRED COMPONENTS Development.Embed)

include(CTest)
enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(installer)
