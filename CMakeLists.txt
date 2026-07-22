cmake_minimum_required(VERSION 3.12)
project(notfm LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)

pkg_check_modules(NOTCURSES REQUIRED IMPORTED_TARGET notcurses)
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.c
    src/core/*.c
    src/pane/*.c
    src/fs/*.c
    src/ui/*.c
    src/config/*.c
    src/utils/*.c
    include/inih/*.c
)

add_executable(notfm ${SOURCES})

target_include_directories(notfm PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(notfm PRIVATE
    PkgConfig::NOTCURSES
    PkgConfig::VIPS
)
