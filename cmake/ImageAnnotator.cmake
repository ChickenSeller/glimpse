# kImageAnnotator (LGPL-3.0, from ksnip) and its kColorPicker dependency: the
# annotation editor behind "Edit". Built as shared libraries so the LGPL
# components stay replaceable.
#
# kImageAnnotator is kept in third_party/kImageAnnotator with Glimpse's own
# changes and tools (see UPSTREAM.md there), so an installed one cannot stand
# in for it. kColorPicker is fetched as published.
#
# Defines GLIMPSE_ANNOTATOR_TARGET, GLIMPSE_ANNOTATOR_RUNTIME (target files to
# ship next to the executable) and GLIMPSE_ANNOTATOR_QM_DIR (its .qm files).

include(FetchContent)

set(GLIMPSE_ANNOTATOR_TARGET "")
set(GLIMPSE_ANNOTATOR_RUNTIME "")
set(GLIMPSE_ANNOTATOR_QM_DIR "")

message(STATUS "Glimpse: building kImageAnnotator from third_party")

set(BUILD_WITH_QT6 ON CACHE BOOL "" FORCE)
set(BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(_glimpse_shared_libs ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS ON)

# kImageAnnotator looks for find_package(kColorPicker-Qt6); the redirect makes
# that resolve to this in-tree build.
FetchContent_Declare(kColorPicker-Qt6
    URL https://github.com/ksnip/kColorPicker/archive/refs/tags/v0.3.1.tar.gz
    URL_HASH SHA256=e78c785ec4a8a22a48a91835c97601f5704b5076b154415353b0d2697dc0b4f7
    # Exports for Windows, see the script. The revision makes existing build
    # trees fetch and patch it afresh.
    PATCH_COMMAND ${CMAKE_COMMAND} -DGLIMPSE_PATCH_REVISION=1
        -DEXPORT_HEADER=include/kColorPicker/KColorPickerExport.h -P "${CMAKE_CURRENT_LIST_DIR}/PatchExportHeader.cmake"
    OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(kColorPicker-Qt6)
# What its export header looks for; its own CMakeLists defines
# KIMAGEANNOTATOR_LIB instead.
target_compile_definitions(kColorPicker PRIVATE KCOLORPICKER_LIB)
# Only an installed kColorPicker defines this namespaced name.
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/kcolorpicker-qt6-extra.cmake" "
if(NOT TARGET kColorPicker::kColorPicker)
    add_library(kColorPicker::kColorPicker ALIAS kColorPicker)
endif()
")

set(_glimpse_annotator_build "${CMAKE_BINARY_DIR}/kimageannotator")
add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/kImageAnnotator" "${_glimpse_annotator_build}")

set(BUILD_SHARED_LIBS ${_glimpse_shared_libs})

set(GLIMPSE_ANNOTATOR_TARGET kImageAnnotator::kImageAnnotator)
set(GLIMPSE_ANNOTATOR_RUNTIME kImageAnnotator kColorPicker)
set(GLIMPSE_ANNOTATOR_QM_DIR "${_glimpse_annotator_build}/translations")
