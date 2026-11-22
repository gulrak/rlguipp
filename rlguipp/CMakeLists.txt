if(TARGET raylib_static)
    set(_rlguippRaylibDefault raylib_static)
else()
    set(_rlguippRaylibDefault raylib)
endif()
set(RLGUIPP_RAYLIB_TARGET ${_rlguippRaylibDefault} CACHE STRING "Name of the raylib target rlguipp links against")

add_library(rlguipp STATIC
    rlguipp.cpp rlguipp.hpp
    rlguipp_filedialog.cpp
    rlguipp_filedialog.hpp
    rlguipp_leds.cpp
    rlguipp_leds.hpp
    rlguipp_codeeditor.cpp
    rlguipp_codeeditor.hpp
    rlguipp_filetree.cpp
    rlguipp_filetree.hpp
    rlguipp_detail.hpp
    rlguiapp.cpp
    rlguiapp.hpp
    rlguipp_font.cpp
    rlguipp_font.hpp
    raygui4.5.h icons.h
    syshelper.cpp
    syshelper.hpp)
add_library(rlguipp::rlguipp ALIAS rlguipp)
target_include_directories(rlguipp PUBLIC ${PROJECT_SOURCE_DIR})
# ghc/utf8.hpp is only used by the implementation
target_include_directories(rlguipp PRIVATE ${PROJECT_SOURCE_DIR}/external)
target_link_libraries(rlguipp PUBLIC ${RLGUIPP_RAYLIB_TARGET} ghc_filesystem)
if(RLGUIPP_GHC_FS_IMPL)
    target_sources(rlguipp PRIVATE ${PROJECT_SOURCE_DIR}/test/fs_impl.cpp)
endif()
