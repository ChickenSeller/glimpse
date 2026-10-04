# kImageAnnotator (LGPL-3.0, from ksnip) and its kColorPicker dependency: the
# annotation editor behind "Edit". Built as shared libraries so the LGPL
# components stay replaceable.
#
# Defines GLIMPSE_ANNOTATOR_TARGET, GLIMPSE_ANNOTATOR_RUNTIME (target files to
# ship next to the executable) and GLIMPSE_ANNOTATOR_QM_DIR (its .qm files).

include(FetchContent)

set(GLIMPSE_ANNOTATOR_TARGET "")
set(GLIMPSE_ANNOTATOR_RUNTIME "")
set(GLIMPSE_ANNOTATOR_QM_DIR "")

find_package(kImageAnnotator-Qt6 QUIET)
if(kImageAnnotator-Qt6_FOUND)
    message(STATUS "Glimpse: using installed kImageAnnotator")
    set(GLIMPSE_ANNOTATOR_TARGET kImageAnnotator::kImageAnnotator)
    return()
endif()

message(STATUS "Glimpse: building kImageAnnotator from source")

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
    OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(kColorPicker-Qt6)
# Only an installed kColorPicker defines this namespaced name.
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/kcolorpicker-qt6-extra.cmake" "
if(NOT TARGET kColorPicker::kColorPicker)
    add_library(kColorPicker::kColorPicker ALIAS kColorPicker)
endif()
")

FetchContent_Declare(kImageAnnotator
    URL https://github.com/ksnip/kImageAnnotator/archive/refs/tags/v0.7.2.tar.gz
    URL_HASH SHA256=7eb593d975b1590a184354ef68dbc3c26479d58eaea00de461d73695176f623c
    # Its canvas painting (checkerboard, frame of the saved area). The
    # revision makes existing build trees fetch and patch it afresh.
    PATCH_COMMAND ${CMAKE_COMMAND} -DGLIMPSE_PATCH_REVISION=6 -P "${CMAKE_CURRENT_LIST_DIR}/PatchImageAnnotator.cmake"
)
FetchContent_MakeAvailable(kImageAnnotator)

set(BUILD_SHARED_LIBS ${_glimpse_shared_libs})

set(GLIMPSE_ANNOTATOR_TARGET kImageAnnotator::kImageAnnotator)
set(GLIMPSE_ANNOTATOR_RUNTIME kImageAnnotator kColorPicker)
set(GLIMPSE_ANNOTATOR_QM_DIR "${kimageannotator_BINARY_DIR}/translations")
