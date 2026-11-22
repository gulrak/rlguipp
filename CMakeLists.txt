cmake_minimum_required(VERSION 3.24)
project(rlguipp VERSION 0.5.0 LANGUAGES C CXX)

# rlguipp - an immediate-mode layout wrapper around raygui, see README.md
#
# Standalone:  cmake -S . -B build -G Ninja && cmake --build build
# As a dependency (FetchContent or add_subdirectory) only the library is built.
# A project that wants other versions of raylib or ghc::filesystem declares them
# (FetchContent_Declare with the names "raylib" and "ghcfilesystem") or creates
# the targets before rlguipp is made available.

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

option(RLGUIPP_BUILD_EXAMPLES "Build the rlguipp showcase" ${PROJECT_IS_TOP_LEVEL})
option(RLGUIPP_BUILD_TESTS "Build the rlguipp tests" ${PROJECT_IS_TOP_LEVEL})
option(RLGUIPP_BUILD_TOOLS "Build the rlguipp tools (font generator)" ${PROJECT_IS_TOP_LEVEL})
option(RLGUIPP_GHC_FS_IMPL "Compile the ghc::filesystem implementation into rlguipp (off if the application provides it)" OFF)

include(cmake/Dependencies.cmake)

add_subdirectory(rlguipp)

if(RLGUIPP_BUILD_TESTS AND NOT EMSCRIPTEN)
    enable_testing()
endif()
if(RLGUIPP_BUILD_EXAMPLES OR RLGUIPP_BUILD_TESTS)
    rlguipp_fetch_fmt()
endif()
if(RLGUIPP_BUILD_TESTS)
    add_subdirectory(test)
endif()
if(RLGUIPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(RLGUIPP_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
