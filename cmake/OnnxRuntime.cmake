# ONNX Runtime (MIT) for the PaddleOCR engine.
#
# Glimpse loads the library at run time through its C API, so the build only
# needs onnxruntime_c_api.h. An installed copy is used when found (Linux
# packages); otherwise the official prebuilt release is downloaded and its
# shared library is copied next to the executable.
#
# Defines GLIMPSE_ORT_INCLUDE_DIR (empty if unavailable) and
# GLIMPSE_ORT_RUNTIME_FILES (libraries to ship with the executable).

include(FetchContent)

set(GLIMPSE_ORT_INCLUDE_DIR "")
set(GLIMPSE_ORT_RUNTIME_FILES "")

find_path(GLIMPSE_ORT_SYSTEM_INCLUDE onnxruntime_c_api.h PATH_SUFFIXES onnxruntime onnxruntime/core/session)
if(GLIMPSE_ORT_SYSTEM_INCLUDE)
    message(STATUS "Glimpse: using installed ONNX Runtime headers in ${GLIMPSE_ORT_SYSTEM_INCLUDE}")
    set(GLIMPSE_ORT_INCLUDE_DIR "${GLIMPSE_ORT_SYSTEM_INCLUDE}")
    return()
endif()

set(_ort_version 1.30.0)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_ort_url https://github.com/microsoft/onnxruntime/releases/download/v${_ort_version}/onnxruntime-win-x64-${_ort_version}.zip)
    set(_ort_hash c6ba983baf5681af108599675d2a89c2d145512d02de28aed0bff177cd0ba949)
    set(_ort_libs onnxruntime.dll onnxruntime_providers_shared.dll)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(_ort_url https://github.com/microsoft/onnxruntime/releases/download/v${_ort_version}/onnxruntime-linux-x64-${_ort_version}.tgz)
    set(_ort_hash a5ed5a3cac51fbb2e90da632ae43d19212faaa20e76484e62bcb7c23ddb3b3fd)
    set(_ort_libs libonnxruntime.so libonnxruntime.so.1 libonnxruntime.so.${_ort_version})
else()
    message(STATUS "Glimpse: no prebuilt ONNX Runtime for this platform; PaddleOCR disabled")
    return()
endif()

FetchContent_Declare(onnxruntime URL ${_ort_url} URL_HASH SHA256=${_ort_hash})
FetchContent_MakeAvailable(onnxruntime)

set(GLIMPSE_ORT_INCLUDE_DIR "${onnxruntime_SOURCE_DIR}/include")
foreach(_lib IN LISTS _ort_libs)
    list(APPEND GLIMPSE_ORT_RUNTIME_FILES "${onnxruntime_SOURCE_DIR}/lib/${_lib}")
endforeach()
