# External dependencies of rlguipp, fetched as release source archives.
# The first FetchContent_Declare() of a name wins, so a parent project can pin other versions,
# and FETCHCONTENT_SOURCE_DIR_<NAME> redirects any of them to a local checkout.

include(FetchContent)

if(NOT TARGET raylib)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    if(EMSCRIPTEN)
        set(PLATFORM "Web" CACHE STRING "" FORCE)
    endif()
    FetchContent_Declare(
        raylib
        URL "https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(raylib)
endif()

if(NOT TARGET ghc_filesystem)
    FetchContent_Declare(
        ghcfilesystem
        URL "https://github.com/gulrak/filesystem/archive/refs/tags/v1.5.14.tar.gz"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(ghcfilesystem)
endif()

# fmt is only used by the showcase and the tests (the StyleManager of the examples)
macro(rlguipp_fetch_fmt)
    if(NOT TARGET fmt::fmt)
        FetchContent_Declare(
            fmt
            URL "https://github.com/fmtlib/fmt/releases/download/11.2.0/fmt-11.2.0.zip"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            EXCLUDE_FROM_ALL
        )
        FetchContent_MakeAvailable(fmt)
    endif()
endmacro()
