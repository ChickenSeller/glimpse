# ONNX Runtime (MIT) for the PaddleOCR engine.
#
# Glimpse loads the library at run time through its C API, so the build only
# needs onnxruntime_c_api.h. An installed copy is used when found (Linux
# packages); otherwise Microsoft's official NuGet package is downloaded and
# its shared library is copied next to the executable. (The prebuilt zips
# on github.com/microsoft/onnxruntime are gone since 2026-11-18; the NuGet
# package holds the same libraries and headers for every platform.)
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
    set(_ort_runtime win-x64)
    set(_ort_libs onnxruntime.dll onnxruntime_providers_shared.dll)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    # Loaded by its path (PaddleOcrEngine), so no soname links are needed.
    set(_ort_runtime linux-x64)
    set(_ort_libs libonnxruntime.so)
else()
    message(STATUS "Glimpse: no prebuilt ONNX Runtime for this platform; PaddleOCR disabled")
    return()
endif()

# A .nupkg is a zip; the name tells the download step to extract it.
FetchContent_Declare(onnxruntime
    URL https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/${_ort_version}/microsoft.ml.onnxruntime.${_ort_version}.nupkg
    URL_HASH SHA256=30df119bd9164eaa187666d93147448fdd4a9a1a3caf83f1505daf7af2d1c5d2
    DOWNLOAD_NAME onnxruntime-${_ort_version}.zip
)
FetchContent_MakeAvailable(onnxruntime)

set(GLIMPSE_ORT_INCLUDE_DIR "${onnxruntime_SOURCE_DIR}/build/native/include")
foreach(_lib IN LISTS _ort_libs)
    list(APPEND GLIMPSE_ORT_RUNTIME_FILES "${onnxruntime_SOURCE_DIR}/runtimes/${_ort_runtime}/native/${_lib}")
endforeach()
