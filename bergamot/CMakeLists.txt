# glimpse-bergamot: Mozilla's Bergamot translation engine (Firefox's offline
# translations) as a shared library with a C interface. Built separately from
# Glimpse (with MSVC on Windows, which the engine supports) and loaded by it at
# run time.
#
#   cmake -S bergamot -B <dir> -DBERGAMOT_SOURCE_DIR=<mozilla/translations>/inference
cmake_minimum_required(VERSION 3.21)
project(glimpse_bergamot CXX C)

set(BERGAMOT_SOURCE_DIR "" CACHE PATH "The inference/ directory of mozilla/translations (with submodules)")
if(NOT EXISTS "${BERGAMOT_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set BERGAMOT_SOURCE_DIR to mozilla/translations/inference")
endif()

# The engine builds with the static runtime (/MT); everything here matches,
# and the DLL then needs no Visual C++ redistributable on the user's machine.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Sources are UTF-8. Without this, MSVC reads them in the system code page
# (936 on Chinese Windows), warns (C4819), and Marian treats warnings as errors.
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Eigen (pulled in by Marian) is MPL-2.0 except for a few LGPL files; this
# makes any use of those a compile error, so the library holds MPL-2.0 Eigen
# code only (see THIRD_PARTY_NOTICES.md).
add_compile_definitions(EIGEN_MPL2_ONLY)

# PCRE2 for the sentence splitter. Its own download script names the MSVC
# library wrongly, so it is built here and handed over as a target.
include(FetchContent)
set(PCRE2_BUILD_PCRE2GREP OFF CACHE BOOL "" FORCE)
set(PCRE2_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_JIT ON CACHE BOOL "" FORCE)
set(PCRE2_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(pcre2
    URL https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.44/pcre2-10.44.tar.gz
    URL_HASH SHA256=86b9cb0aa3bcb7994faa88018292bc704cdbb708e785f7c74352ff6ea7d3175b
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(pcre2)
set(SSPLIT_USE_INTERNAL_PCRE2 OFF CACHE BOOL "" FORCE)
set(PCRE2_LIBRARIES pcre2-8-static CACHE STRING "" FORCE)
set(PCRE2_INCLUDE_DIRS "${pcre2_BINARY_DIR}" CACHE PATH "" FORCE)

# Native CPU build of the library only: no tests, apps or WebAssembly.
set(COMPILE_WASM OFF CACHE BOOL "" FORCE)
set(COMPILE_TESTS OFF CACHE BOOL "" FORCE)
set(COMPILE_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(USE_WASM_COMPATIBLE_SOURCE ON CACHE BOOL "" FORCE)
set(COMPILE_LIBRARY_ONLY ON CACHE BOOL "" FORCE)
set(USE_MKL OFF CACHE BOOL "" FORCE)
set(USE_FBGEMM OFF CACHE BOOL "" FORCE)
# Runs on any x86-64 CPU; intgemm still picks AVX2/AVX-512 at run time.
set(BUILD_ARCH "x86-64-v2" CACHE STRING "" FORCE)
add_subdirectory("${BERGAMOT_SOURCE_DIR}" bergamot EXCLUDE_FROM_ALL)

add_library(glimpse-bergamot SHARED glimpse_bergamot.cpp glimpse_bergamot.h)
target_compile_features(glimpse-bergamot PRIVATE cxx_std_17)
target_compile_definitions(glimpse-bergamot PRIVATE GLIMPSE_BERGAMOT_BUILD)
target_include_directories(glimpse-bergamot PRIVATE "${BERGAMOT_SOURCE_DIR}/src")
target_link_libraries(glimpse-bergamot PRIVATE bergamot-translator-source)
