cmake_minimum_required(VERSION 3.13)

# When building as part of a Pico SDK project the toolchain file and SDK
# import must happen *before* project().  Users integrating this library into
# a Pico SDK build should add this directory via add_subdirectory() from their
# own CMakeLists.txt (where pico_sdk_init() has already been called).
#
# When building standalone on a desktop host, configure normally:
#   cmake -S . -B build && cmake --build build

project(tasd
    VERSION 1.0.0
    LANGUAGES C
    DESCRIPTION "Minimal C library for the TASD file format"
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- Library target ----------------------------------------------------------

add_library(tasd STATIC
    src/tasd.c
)

target_include_directories(tasd PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Strict warnings when building as the top-level project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    if(MSVC)
        target_compile_options(tasd PRIVATE /W4 /WX)
    else()
        target_compile_options(tasd PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

# ---- Tests (desktop only) ----------------------------------------------------

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- Install rules -----------------------------------------------------------

include(GNUInstallDirs)

install(TARGETS tasd
    EXPORT tasdTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES include/tasd.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
