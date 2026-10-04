# Firefox's offline translation engine (Mozilla Bergamot, MPL-2.0) as
# glimpse-bergamot.dll, built from bergamot/ by MSVC (the engine does not
# build with MinGW) and loaded by Glimpse at run time through a C interface.
#
# Windows only for now, and only when Visual Studio 2022 is installed.
# Defines GLIMPSE_BERGAMOT_LIBRARY (empty if not built).

set(GLIMPSE_BERGAMOT_LIBRARY "")

if(NOT WIN32)
    message(STATUS "Glimpse: Firefox translation engine is built on Windows only so far")
    return()
endif()
cmake_host_system_information(RESULT _vs_dir QUERY VS_17_DIR)
if(NOT _vs_dir)
    message(STATUS "Glimpse: Visual Studio 2022 not found; Firefox translation engine not built")
    return()
endif()
find_package(Git QUIET)
if(NOT GIT_FOUND)
    message(STATUS "Glimpse: git not found; Firefox translation engine not built")
    return()
endif()

# mozilla/translations at a fixed commit: only its inference/ directory and
# the submodules the native engine needs (not the training pipeline, not the
# WebAssembly toolchain). Kept under build/ beside the sources rather than in
# the build directory: MSVC still limits paths to 260 characters, and the
# engine's object files nest deeply.
set(_bergamot_commit 69455acaecbe8650cdba988dbcf7c10ca20e7c48)
set(_bergamot_src "${CMAKE_SOURCE_DIR}/build/bergamot-src")
set(_bergamot_bin "${CMAKE_SOURCE_DIR}/build/bergamot-build")
set(_bergamot_submodules
    inference/3rd_party/ssplit-cpp
    inference/marian-fork/src/3rd_party/sentencepiece
    inference/marian-fork/src/3rd_party/intgemm
    inference/marian-fork/src/3rd_party/ruy
    inference/marian-fork/src/3rd_party/simd_utils)
if(NOT EXISTS "${_bergamot_src}/inference/marian-fork/src/3rd_party/sentencepiece/CMakeLists.txt")
    message(STATUS "Glimpse: fetching the Firefox translation engine sources (mozilla/translations)")
    file(REMOVE_RECURSE "${_bergamot_src}")
    execute_process(COMMAND ${GIT_EXECUTABLE} -c core.longpaths=true clone -q --filter=blob:none --sparse --no-checkout
                            https://github.com/mozilla/translations.git "${_bergamot_src}"
                    RESULT_VARIABLE _git_result)
    if(_git_result EQUAL 0)
        execute_process(COMMAND ${GIT_EXECUTABLE} config core.longpaths true WORKING_DIRECTORY "${_bergamot_src}")
        execute_process(COMMAND ${GIT_EXECUTABLE} sparse-checkout set inference WORKING_DIRECTORY "${_bergamot_src}")
        execute_process(COMMAND ${GIT_EXECUTABLE} checkout -q ${_bergamot_commit}
                        WORKING_DIRECTORY "${_bergamot_src}" RESULT_VARIABLE _git_result)
    endif()
    if(_git_result EQUAL 0)
        execute_process(COMMAND ${GIT_EXECUTABLE} -c core.longpaths=true submodule update --init --depth 1 ${_bergamot_submodules}
                        WORKING_DIRECTORY "${_bergamot_src}" RESULT_VARIABLE _git_result)
    endif()
    if(NOT _git_result EQUAL 0)
        message(WARNING "Glimpse: could not fetch the Firefox translation engine; it is not built")
        return()
    endif()
endif()

# Eigen (pulled in by Marian) probes for a Fortran compiler by generating a
# test project; under the Visual Studio generator that is an Intel Fortran
# .vfproj, and Windows pops up an "open with" dialog for it on every
# configure. Nothing here uses Fortran: report it as unavailable instead.
file(GLOB _eigen_fortran_probes
    "${_bergamot_src}/inference/marian-fork/src/3rd_party/onnxjs/deps/eigen/blas/CMakeLists.txt"
    "${_bergamot_src}/inference/marian-fork/src/3rd_party/onnxjs/deps/eigen/lapack/CMakeLists.txt"
    "${_bergamot_src}/inference/marian-fork/src/3rd_party/onnxjs/deps/eigen/test/CMakeLists.txt")
foreach(_file IN LISTS _eigen_fortran_probes)
    file(READ "${_file}" _text)
    string(REPLACE "workaround_9220(Fortran EIGEN_Fortran_COMPILER_WORKS)"
                   "set(EIGEN_Fortran_COMPILER_WORKS OFF) # Glimpse: no Fortran probe" _patched "${_text}")
    if(NOT _patched STREQUAL _text)
        file(WRITE "${_file}" "${_patched}")
    endif()
endforeach()

include(ExternalProject)
# Its own MSVC build; 8 threads at most (the engine's flags add /MP, so the
# projects build one at a time with 8 compiler processes each).
ExternalProject_Add(glimpse_bergamot
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/bergamot"
    BINARY_DIR "${_bergamot_bin}"
    CMAKE_GENERATOR "Visual Studio 17 2022"
    CMAKE_GENERATOR_PLATFORM x64
    CMAKE_ARGS -DBERGAMOT_SOURCE_DIR=${_bergamot_src}/inference -DGIT_SUBMODULE=OFF
    BUILD_COMMAND ${CMAKE_COMMAND} -E env CL=/MP8
                  ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target glimpse-bergamot --parallel 1
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS "${_bergamot_bin}/Release/glimpse-bergamot.dll"
    BUILD_ALWAYS ON # cheap when up to date; picks up changes in bergamot/
)
set(GLIMPSE_BERGAMOT_LIBRARY "${_bergamot_bin}/Release/glimpse-bergamot.dll")
message(STATUS "Glimpse: Firefox translation engine (Bergamot) will be built with MSVC")
