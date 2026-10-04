# llama.cpp (MIT) for local, offline translation with a GGUF language model.
#
# Like ONNX Runtime, Glimpse loads llama.cpp at run time through its C API, so
# the build needs only the headers (from the matching source release). The
# official prebuilt Vulkan build is used: it runs on NVIDIA, AMD and Intel
# GPUs without CUDA and falls back to the CPU (it ships CPU backends for each
# instruction set and picks the best one at run time).
#
# Defines GLIMPSE_LLAMA_INCLUDE_DIRS (empty if unavailable) and
# GLIMPSE_LLAMA_RUNTIME_FILES (libraries to ship next to the executable).

include(FetchContent)

set(GLIMPSE_LLAMA_INCLUDE_DIRS "")
set(GLIMPSE_LLAMA_RUNTIME_FILES "")

set(_llama_build b11387)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_llama_url https://github.com/ggml-org/llama.cpp/releases/download/${_llama_build}/llama-${_llama_build}-bin-win-vulkan-x64.zip)
    set(_llama_hash bed482e577f6b86e2b6a2d15d67cc2dbdd34498ad445168044292be4b7919540)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(_llama_url https://github.com/ggml-org/llama.cpp/releases/download/${_llama_build}/llama-${_llama_build}-bin-ubuntu-vulkan-x64.tar.gz)
    set(_llama_hash 9a9a75a2179c202c0158575fba8d7ecd83af7f2a993ee3fd18fa95919174ceff)
else()
    message(STATUS "Glimpse: no prebuilt llama.cpp for this platform; local translation disabled")
    return()
endif()

# Headers of exactly this build: the C API changes between builds.
# Download and unpack only: SOURCE_SUBDIR names a directory without a
# CMakeLists.txt, so neither archive is built as part of this project.
FetchContent_Declare(llama_headers
    URL https://github.com/ggml-org/llama.cpp/archive/refs/tags/${_llama_build}.tar.gz
    URL_HASH SHA256=44630ee42cf6615e660fa300c16701649173ac12583d84958455fb8d0737175f
    SOURCE_SUBDIR glimpse-headers-only)
FetchContent_Declare(llama_bin URL ${_llama_url} URL_HASH SHA256=${_llama_hash}
    SOURCE_SUBDIR glimpse-binaries-only)
FetchContent_MakeAvailable(llama_headers llama_bin)

set(GLIMPSE_LLAMA_INCLUDE_DIRS "${llama_headers_SOURCE_DIR}/include" "${llama_headers_SOURCE_DIR}/ggml/include")

# The library, ggml and its backends; not the command-line tools' libraries.
if(WIN32)
    file(GLOB _llama_libs "${llama_bin_SOURCE_DIR}/llama.dll" "${llama_bin_SOURCE_DIR}/ggml*.dll"
                          "${llama_bin_SOURCE_DIR}/libomp.dll")
    list(FILTER _llama_libs EXCLUDE REGEX "ggml-rpc")
else()
    file(GLOB_RECURSE _llama_libs "${llama_bin_SOURCE_DIR}/libllama.so*" "${llama_bin_SOURCE_DIR}/libggml*.so*")
endif()
set(GLIMPSE_LLAMA_RUNTIME_FILES ${_llama_libs})
list(LENGTH _llama_libs _llama_count)
message(STATUS "Glimpse: llama.cpp ${_llama_build} (${_llama_count} libraries) for local translation")
