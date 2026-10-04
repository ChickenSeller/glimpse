# Tesseract OCR engine (Apache-2.0) for Glimpse.
#
# Uses an installed Tesseract 5 when CMake can find one (Linux distros ship
# TesseractConfig.cmake). Otherwise Leptonica and Tesseract are downloaded and
# built statically in their smallest form: no image codecs (Glimpse hands over
# raw pixels), no training tools; recognition uses the LSTM engine.
#
# Defines GLIMPSE_TESSERACT_TARGET (empty if Tesseract is unavailable).

include(FetchContent)

set(GLIMPSE_TESSERACT_TARGET "")

find_package(Tesseract 5 CONFIG QUIET)
if(Tesseract_FOUND AND TARGET Tesseract::libtesseract)
    message(STATUS "Glimpse: using installed Tesseract ${Tesseract_VERSION}")
    set(GLIMPSE_TESSERACT_TARGET Tesseract::libtesseract)
    return()
endif()

message(STATUS "Glimpse: building Tesseract from source")

# --- Leptonica --------------------------------------------------------------
foreach(codec ZLIB PNG GIF JPEG TIFF WEBP OPENJPEG)
    set(ENABLE_${codec} OFF CACHE BOOL "" FORCE)
endforeach()
set(BUILD_PROG OFF CACHE BOOL "" FORCE)
set(SW_BUILD OFF CACHE BOOL "" FORCE)

FetchContent_Declare(leptonica
    URL https://github.com/DanBloomberg/leptonica/archive/refs/tags/1.87.0.tar.gz
    URL_HASH SHA256=fa2b40c5caea96d1bb93a97486262aed8731b69ce25a84a6bf5d25323e33f631
    OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(leptonica)
set_property(DIRECTORY ${leptonica_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL TRUE)

# Tesseract looks Leptonica up with find_package(Leptonica CONFIG), which now
# resolves to FetchContent's redirect; provide the variables it then reads.
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/leptonica-extra.cmake" "
set(Leptonica_VERSION 1.87.0)
set(Leptonica_LIBRARIES leptonica)
set(Leptonica_INCLUDE_DIRS \"${leptonica_SOURCE_DIR}/src;${leptonica_BINARY_DIR}/src\")
")

# --- Tesseract --------------------------------------------------------------
set(BUILD_TRAINING_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
# The legacy engine stays in: 5.5.3's CMake drops the legacy sources for
# DISABLED_LEGACY_ENGINE but never defines the macro, so the build cannot link.
set(DISABLED_LEGACY_ENGINE OFF CACHE BOOL "" FORCE)
set(GRAPHICS_DISABLED ON CACHE BOOL "" FORCE)
set(DISABLE_TIFF ON CACHE BOOL "" FORCE)
set(DISABLE_ARCHIVE ON CACHE BOOL "" FORCE)
set(DISABLE_CURL ON CACHE BOOL "" FORCE)
set(OPENMP_BUILD OFF CACHE BOOL "" FORCE)
set(ENABLE_NATIVE OFF CACHE BOOL "" FORCE)
set(ENABLE_CCACHE OFF CACHE BOOL "" FORCE)
set(INSTALL_CONFIGS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(tesseract
    URL https://github.com/tesseract-ocr/tesseract/archive/refs/tags/5.5.3.tar.gz
    URL_HASH SHA256=9218e62793116d42a9f6d14cd9348518b27f382096eea3d0f2d1a24616bb5884
    # Its configure-time TIFF probe links against Leptonica, which an in-tree
    # build has not produced yet; we disable TIFF anyway.
    PATCH_COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_LIST_DIR}/PatchTesseract.cmake"
)
FetchContent_MakeAvailable(tesseract)
# Only libtesseract is needed (built because Glimpse links it), not the CLI.
set_property(DIRECTORY ${tesseract_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL TRUE)

# MinGW GCC cannot keep the stack 32-byte aligned on Win64 (GCC bug 54412),
# yet spills AVX registers with aligned moves; Tesseract's AVX2 kernels then
# crash. Have the assembler emit unaligned moves instead.
if(MINGW)
    target_compile_options(libtesseract PRIVATE -Wa,-muse-unaligned-vector-move)
endif()

# The LSTM recognizer keeps each text line image; by default it encodes them
# as PNG, which this Leptonica (built without libpng) cannot do: it falls back
# to BMP and warns on every line ("png library missing"). Keep them as plain
# images instead, which also skips the encoding.
target_compile_definitions(libtesseract PRIVATE TESSERACT_IMAGEDATA_AS_PIX)
# Each Tesseract instance loads Leptonica's bitmap font for captions on debug
# images (never produced here); the font is stored as TIFF, which this
# Leptonica cannot read, so every engine start logged "bmfCreate" errors.
target_compile_definitions(libtesseract PRIVATE TESSERACT_DISABLE_DEBUG_FONTS)

# The public headers include the generated tesseract/version.h, which an
# in-tree build only has in its binary directory.
target_include_directories(libtesseract INTERFACE $<BUILD_INTERFACE:${tesseract_BINARY_DIR}/include>)

set(GLIMPSE_TESSERACT_TARGET libtesseract)
