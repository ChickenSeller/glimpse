cmake_minimum_required(VERSION 3.24)

# The release pipeline passes the version from the tag (v1.2.3 -> 1.2.3).
set(GLIMPSE_VERSION 0.1.0 CACHE STRING "Version of Glimpse")
project(glimpse VERSION ${GLIMPSE_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt is looked up first: a missing component (or a missing system package Qt
# itself needs, such as the OpenGL headers on Linux) then fails here with a
# clear message, and the third-party projects below reuse this Qt.
# Multimedia: screen recording (QScreenCapture, QVideoFrameInput: Qt 6.8+).
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Svg Concurrent Network Multimedia LinguistTools)
if(UNIX AND NOT APPLE)
    find_package(Qt6 REQUIRED COMPONENTS DBus) # Wayland screenshot portal
endif()

# Third-party code comes before qt_standard_project_setup() so Qt's
# AUTOMOC/AUTOUIC (enabled there) do not reach into its sources. Each
# dependency uses an installed package when CMake finds one (e.g. Linux
# distros) and otherwise downloads a pinned release.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF)

# QR code / barcode decoding: ZXing-C++ (Apache-2.0), readers only.
set(ZXING_READERS ON CACHE BOOL "" FORCE)
set(ZXING_WRITERS OFF CACHE STRING "" FORCE)
set(ZXING_C_API OFF CACHE BOOL "" FORCE)
set(ZXING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZXING_BLACKBOX_TESTS OFF CACHE BOOL "" FORCE)
set(ZXING_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(ZXING_PYTHON_MODULE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ZXing
    URL https://github.com/zxing-cpp/zxing-cpp/archive/refs/tags/v3.1.1.tar.gz
    URL_HASH SHA256=7286b1e6ade66fe82b7c8208b4595deeb55d6486b410834fdc65702f46650542
    FIND_PACKAGE_ARGS 2.2 CONFIG
)
FetchContent_MakeAvailable(ZXing)
if(zxing_SOURCE_DIR)
    # Only the library we link is built, not its other targets.
    set_property(DIRECTORY ${zxing_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL TRUE)
endif()

# Single instance: KDSingleApplication (MIT, KDAB). A second start hands over
# to the running Glimpse, which owns the global hotkeys and the tray icon.
set(KDSingleApplication_QT6 ON CACHE BOOL "" FORCE)
set(KDSingleApplication_STATIC ON CACHE BOOL "" FORCE)
set(KDSingleApplication_EXAMPLES OFF CACHE BOOL "" FORCE)
set(KDSingleApplication_TESTS OFF CACHE BOOL "" FORCE)
set(KDSingleApplication_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(KDSingleApplication
    URL https://github.com/KDAB/KDSingleApplication/releases/download/v1.2.1/kdsingleapplication-1.2.1.tar.gz
    URL_HASH SHA256=e3254ce9dc5ecf6d61ef83264bc61d486a307f0e3c9ed1bb2176f068cdbcbe09
    FIND_PACKAGE_ARGS NAMES KDSingleApplication-qt6
)
FetchContent_MakeAvailable(KDSingleApplication)
if(kdsingleapplication_SOURCE_DIR)
    set_property(DIRECTORY ${kdsingleapplication_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL TRUE)
endif()

# Shown in the About window.
set(GLIMPSE_GITHUB_URL "https://github.com/ChickenSeller/glimpse" CACHE STRING "Glimpse's source repository")
set(GLIMPSE_HOMEPAGE_URL "https://pages.yanlei.org/kaguya/glimpse/" CACHE STRING "Glimpse's homepage")

# Text recognition engines (Windows OCR needs nothing extra).
option(GLIMPSE_WITH_TESSERACT "Build the Tesseract OCR engine" ON)
option(GLIMPSE_WITH_PADDLEOCR "Build the PaddleOCR engine (ONNX Runtime)" ON)
if(GLIMPSE_WITH_TESSERACT)
    include(cmake/Tesseract.cmake)
endif()
if(GLIMPSE_WITH_PADDLEOCR)
    include(cmake/OnnxRuntime.cmake)
endif()

# Local (offline) translation.
option(GLIMPSE_WITH_LLAMA "Build local translation with llama.cpp" ON)
if(GLIMPSE_WITH_LLAMA)
    include(cmake/Llama.cmake)
endif()

# Firefox's offline translation engine (needs Visual Studio on Windows).
option(GLIMPSE_WITH_BERGAMOT "Build Firefox's translation engine (Bergamot)" ON)
if(GLIMPSE_WITH_BERGAMOT)
    include(cmake/Bergamot.cmake)
endif()

# Annotation editor.
include(cmake/ImageAnnotator.cmake)

qt_standard_project_setup()
# Windows gets a portable layout: glimpse.exe at the top of the install tree,
# with DLLs, plugin folders and translations beside it (windeployqt's layout).
if(WIN32)
    set(GLIMPSE_INSTALL_BINDIR ".")
else()
    set(GLIMPSE_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
endif()
set(CMAKE_AUTORCC ON)

# --- OCR engines --------------------------------------------------------------
# A library of its own so tools/tests can run the engines without the UI.
qt_add_library(glimpse_ocr STATIC
    src/app/Language.cpp
    src/app/Language.h
    src/ocr/ModelStore.cpp
    src/ocr/ModelStore.h
    src/ocr/OcrEngine.cpp
    src/ocr/OcrEngine.h
)
target_include_directories(glimpse_ocr PUBLIC src)
target_link_libraries(glimpse_ocr PUBLIC Qt6::Widgets Qt6::Concurrent Qt6::Network)

if(WIN32)
    # Windows' built-in OCR (Windows.Media.Ocr), driven through its WinRT ABI.
    target_sources(glimpse_ocr PRIVATE src/ocr/WindowsOcrEngine.cpp src/ocr/WindowsOcrEngine.h)
    target_link_libraries(glimpse_ocr PRIVATE runtimeobject)
    target_compile_definitions(glimpse_ocr PRIVATE GLIMPSE_HAVE_WINDOWS_OCR)
endif()

if(GLIMPSE_TESSERACT_TARGET)
    target_sources(glimpse_ocr PRIVATE src/ocr/TesseractEngine.cpp src/ocr/TesseractEngine.h)
    target_link_libraries(glimpse_ocr PRIVATE ${GLIMPSE_TESSERACT_TARGET})
    target_compile_definitions(glimpse_ocr PRIVATE GLIMPSE_HAVE_TESSERACT)
endif()

if(GLIMPSE_ORT_INCLUDE_DIR)
    target_sources(glimpse_ocr PRIVATE src/ocr/PaddleOcrEngine.cpp src/ocr/PaddleOcrEngine.h)
    target_include_directories(glimpse_ocr PRIVATE ${GLIMPSE_ORT_INCLUDE_DIR})
    target_compile_definitions(glimpse_ocr PRIVATE GLIMPSE_HAVE_PADDLE)
endif()

# --- application --------------------------------------------------------------
qt_add_executable(glimpse
    src/main.cpp

    src/app/AboutDialog.cpp
    src/app/AboutDialog.h
    src/app/AboutDialog.ui
    src/app/AppIcon.cpp
    src/app/AppIcon.h
    src/app/AppSettings.cpp
    src/app/AppSettings.h
    src/app/CaptureController.cpp
    src/app/CaptureController.h
    src/app/CaptureMode.h
    src/app/CaptureToolbar.cpp
    src/app/CaptureToolbar.h
    src/app/CaptureToolbar.ui
    src/app/DelayCountdown.cpp
    src/app/DelayCountdown.h
    src/app/TintedIcon.cpp
    src/app/TintedIcon.h

    src/capture/CursorCapture.h
    src/capture/DesktopSnapshot.cpp
    src/capture/DesktopSnapshot.h
    src/capture/Platform.cpp
    src/capture/Platform.h
    src/capture/QtScreenGrabber.cpp
    src/capture/QtScreenGrabber.h
    src/capture/ScreenGrabber.cpp
    src/capture/ScreenGrabber.h
    src/capture/ScrollCaptureSession.cpp
    src/capture/ScrollCaptureSession.h
    src/capture/ScrollInput.h
    src/capture/ScrollStitcher.cpp
    src/capture/ScrollStitcher.h
    src/capture/WindowList.h

    src/hotkey/GlobalHotkeys.h

    src/overlay/RegionSelector.cpp
    src/overlay/RegionSelector.h

    src/pin/PinWindow.cpp
    src/pin/PinWindow.h

    src/editor/AnnotatorWindow.cpp
    src/editor/AnnotatorWindow.h
    src/editor/AnnotatorWindow.ui
    src/color/ColorResultDialog.cpp
    src/color/ColorResultDialog.h
    src/color/ColorResultDialog.ui

    src/editor/EditorWindow.cpp
    src/editor/EditorWindow.h
    src/editor/EditorWindow.ui
    src/editor/ImageActions.cpp
    src/editor/ImageActions.h

    src/ocr/OcrResultDialog.cpp
    src/ocr/OcrResultDialog.h
    src/ocr/OcrResultDialog.ui

    src/qr/BarcodeScanner.cpp
    src/qr/BarcodeScanner.h
    src/qr/QrResultDialog.cpp
    src/qr/QrResultDialog.h
    src/qr/QrResultDialog.ui

    src/record/InputMonitor.h
    src/record/KeyboardOverlay.cpp
    src/record/KeyboardOverlay.h
    src/record/RecordingDoneDialog.cpp
    src/record/RecordingDoneDialog.h
    src/record/RecordingDoneDialog.ui
    src/record/RecordingPanel.ui
    src/record/RecordingSession.cpp
    src/record/RecordingSession.h
    src/record/ScreenRecorder.cpp
    src/record/ScreenRecorder.h

    src/settings/HotkeyEdit.cpp
    src/settings/HotkeyEdit.h
    src/settings/SettingsDialog.cpp
    src/settings/SettingsDialog.h
    src/settings/SettingsDialog.ui

    src/translate/FirefoxTranslation.cpp
    src/translate/FirefoxTranslation.h
    src/translate/LocalModel.cpp
    src/translate/LocalModel.h
    src/translate/TranslateResultDialog.cpp
    src/translate/TranslateResultDialog.h
    src/translate/TranslateResultDialog.ui
    src/translate/Translator.cpp
    src/translate/Translator.h

    resources/glimpse.qrc
)

target_include_directories(glimpse PRIVATE src)
target_compile_definitions(glimpse PRIVATE GLIMPSE_VERSION="${PROJECT_VERSION}"
    GLIMPSE_GITHUB_URL="${GLIMPSE_GITHUB_URL}" GLIMPSE_HOMEPAGE_URL="${GLIMPSE_HOMEPAGE_URL}")
# UI translations, compiled to .qm and embedded under :/i18n. Refresh the .ts
# files after changing UI strings with: cmake --build <dir> --target update_translations
qt_add_translations(TARGETS glimpse
    SOURCE_TARGETS glimpse glimpse_ocr
    TS_FILES
        translations/glimpse_zh_CN.ts
        translations/glimpse_ja.ts
)

# Svg: the SVG icon engine plugin that renders the Material Symbols icons.
target_link_libraries(glimpse PRIVATE glimpse_ocr Qt6::Widgets Qt6::Svg Qt6::Concurrent Qt6::Multimedia Qt6::Network ZXing::ZXing
                                      KDAB::kdsingleapplication ${GLIMPSE_ANNOTATOR_TARGET})

# Window/object picking and global hotkeys: Win32 only so far.
if(WIN32)
    # The file's icon and version details.
    enable_language(RC)
    configure_file(resources/glimpse.rc.in ${CMAKE_CURRENT_BINARY_DIR}/glimpse.rc @ONLY)
    target_sources(glimpse PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/glimpse.rc)
    target_sources(glimpse PRIVATE
        src/capture/CursorCapture_win.cpp
        src/capture/ScrollInput_win.cpp
        src/capture/WindowList_win.cpp
        src/hotkey/GlobalHotkeys_win.cpp
        src/record/InputMonitor_win.cpp
    )
    target_link_libraries(glimpse PRIVATE dwmapi)
else()
    target_sources(glimpse PRIVATE
        src/capture/CursorCapture_stub.cpp
        src/capture/ScrollInput_stub.cpp
        src/capture/WindowList_stub.cpp
        src/hotkey/GlobalHotkeys_stub.cpp
        src/record/InputMonitor_stub.cpp
    )
endif()

# Linux: Wayland screenshots go through xdg-desktop-portal over D-Bus.
if(UNIX AND NOT APPLE)
    target_sources(glimpse PRIVATE
        src/capture/PortalScreenGrabber.cpp
        src/capture/PortalScreenGrabber.h
    )
    target_link_libraries(glimpse PRIVATE Qt6::DBus)
    target_compile_definitions(glimpse PRIVATE GLIMPSE_HAVE_PORTAL)
endif()

set_target_properties(glimpse PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# The Firefox engine's C interface (the library itself is loaded at run time).
target_include_directories(glimpse PRIVATE bergamot)
if(GLIMPSE_BERGAMOT_LIBRARY)
    add_dependencies(glimpse glimpse_bergamot)
    add_custom_command(TARGET glimpse POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GLIMPSE_BERGAMOT_LIBRARY}" "$<TARGET_FILE_DIR:glimpse>"
        VERBATIM)
endif()

# llama.cpp is loaded at run time from next to the executable; the build only
# needs its headers.
if(GLIMPSE_LLAMA_INCLUDE_DIRS)
    target_include_directories(glimpse PRIVATE ${GLIMPSE_LLAMA_INCLUDE_DIRS})
    target_compile_definitions(glimpse PRIVATE GLIMPSE_HAVE_LLAMA)
endif()
foreach(_lib IN LISTS GLIMPSE_LLAMA_RUNTIME_FILES)
    add_custom_command(TARGET glimpse POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_lib}" "$<TARGET_FILE_DIR:glimpse>"
        VERBATIM)
endforeach()

# The downloaded ONNX Runtime library lives next to the executable, where
# PaddleOcrEngine looks for it first.
foreach(_lib IN LISTS GLIMPSE_ORT_RUNTIME_FILES)
    add_custom_command(TARGET glimpse POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_lib}" "$<TARGET_FILE_DIR:glimpse>"
        VERBATIM)
endforeach()

# The in-tree kImageAnnotator/kColorPicker DLLs and the editor's translations
# (loaded from translations/ next to the executable by Language::apply).
foreach(_lib IN LISTS GLIMPSE_ANNOTATOR_RUNTIME)
    add_custom_command(TARGET glimpse POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${_lib}>" "$<TARGET_FILE_DIR:glimpse>"
        VERBATIM)
endforeach()
if(GLIMPSE_ANNOTATOR_QM_DIR)
    add_dependencies(glimpse translations) # kImageAnnotator's .qm target
    foreach(_lang zh_CN ja)
        set(_qm "${GLIMPSE_ANNOTATOR_QM_DIR}/kImageAnnotator_${_lang}.qm")
        add_custom_command(TARGET glimpse POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:glimpse>/translations"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_qm}" "$<TARGET_FILE_DIR:glimpse>/translations"
            VERBATIM)
    endforeach()
endif()

# --- tools ----------------------------------------------------------------------
# glimpse-ocr-test: renders sample text and runs every OCR engine on it.
qt_add_executable(glimpse-ocr-test tests/OcrSmokeTest.cpp)
target_link_libraries(glimpse-ocr-test PRIVATE glimpse_ocr)

# glimpse-stitch-test: checks scroll-capture stitching on synthetic pages.
qt_add_executable(glimpse-stitch-test tests/ScrollStitcherTest.cpp src/capture/ScrollStitcher.cpp)
target_include_directories(glimpse-stitch-test PRIVATE src)
target_link_libraries(glimpse-stitch-test PRIVATE Qt6::Gui)

# glimpse-scroll-test: end-to-end scrolling capture on a window it owns
# (Windows only: needs wheel input injection). Moves the mouse while running.
if(WIN32)
    qt_add_executable(glimpse-scroll-test
        tests/ScrollCaptureTest.cpp
        src/capture/Platform.cpp
        src/capture/ScrollCaptureSession.cpp
        src/capture/ScrollInput_win.cpp
        src/capture/ScrollStitcher.cpp
    )
    target_include_directories(glimpse-scroll-test PRIVATE src)
    target_link_libraries(glimpse-scroll-test PRIVATE Qt6::Widgets dwmapi)
endif()

# glimpse-translate-test: local (llama.cpp) translation of sample text with
# the model the settings select (it must be downloaded already).
if(GLIMPSE_LLAMA_INCLUDE_DIRS)
    qt_add_executable(glimpse-translate-test
        tests/TranslateTest.cpp
        src/app/AppSettings.cpp
        src/translate/FirefoxTranslation.cpp
        src/translate/LocalModel.cpp
        src/translate/Translator.cpp
    )
    target_include_directories(glimpse-translate-test PRIVATE src bergamot ${GLIMPSE_LLAMA_INCLUDE_DIRS})
    target_compile_definitions(glimpse-translate-test PRIVATE GLIMPSE_HAVE_LLAMA)
    target_link_libraries(glimpse-translate-test PRIVATE glimpse_ocr Qt6::Widgets Qt6::Network Qt6::Concurrent)
endif()

# glimpse-record-test: records a few seconds of the screen to an MP4 and reads
# it back.
qt_add_executable(glimpse-record-test
    tests/RecordTest.cpp
    src/capture/DesktopSnapshot.cpp
    src/capture/Platform.cpp
    src/record/InputMonitor.h
    src/record/KeyboardOverlay.cpp
    src/record/KeyboardOverlay.h
    src/record/ScreenRecorder.cpp
    src/record/ScreenRecorder.h
)
if(WIN32)
    target_sources(glimpse-record-test PRIVATE src/capture/CursorCapture_win.cpp src/record/InputMonitor_win.cpp)
    target_link_libraries(glimpse-record-test PRIVATE dwmapi)
else()
    target_sources(glimpse-record-test PRIVATE src/capture/CursorCapture_stub.cpp src/record/InputMonitor_stub.cpp)
endif()
target_include_directories(glimpse-record-test PRIVATE src)
target_link_libraries(glimpse-record-test PRIVATE Qt6::Widgets Qt6::Multimedia)

# --- distribution -----------------------------------------------------------------
# Everything a user needs goes into the "glimpse" install component; the
# third-party projects' own install rules (headers, static libs, CMake configs)
# stay in other components and are not shipped.
#
#   cmake --install <build dir> --component glimpse --prefix <dir>   folder
#   cmake --build <build dir> --target package                         zip
#
# Build the Release configuration for this; a Debug build deploys debug Qt.
install(TARGETS glimpse
    BUNDLE DESTINATION . COMPONENT glimpse
    RUNTIME DESTINATION ${GLIMPSE_INSTALL_BINDIR} COMPONENT glimpse
)
install(FILES ${GLIMPSE_ORT_RUNTIME_FILES} DESTINATION ${GLIMPSE_INSTALL_BINDIR} COMPONENT glimpse)
install(FILES ${GLIMPSE_LLAMA_RUNTIME_FILES} DESTINATION ${GLIMPSE_INSTALL_BINDIR} COMPONENT glimpse)
# Glimpse's license and those of everything it ships (see THIRD_PARTY_NOTICES.md).
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION ${GLIMPSE_INSTALL_BINDIR} COMPONENT glimpse)
install(DIRECTORY licenses/ DESTINATION ${GLIMPSE_INSTALL_BINDIR}/licenses COMPONENT glimpse)
if(GLIMPSE_BERGAMOT_LIBRARY)
    install(FILES ${GLIMPSE_BERGAMOT_LIBRARY} DESTINATION ${GLIMPSE_INSTALL_BINDIR} COMPONENT glimpse)
endif()
set(_glimpse_extra_libs "")
foreach(_lib IN LISTS GLIMPSE_ANNOTATOR_RUNTIME)
    install(FILES "$<TARGET_FILE:${_lib}>" DESTINATION ${GLIMPSE_INSTALL_BINDIR} COMPONENT glimpse)
    list(APPEND _glimpse_extra_libs "$<TARGET_FILE:${_lib}>")
endforeach()
if(GLIMPSE_ANNOTATOR_QM_DIR)
    install(FILES "${GLIMPSE_ANNOTATOR_QM_DIR}/kImageAnnotator_zh_CN.qm" "${GLIMPSE_ANNOTATOR_QM_DIR}/kImageAnnotator_ja.qm"
            DESTINATION ${GLIMPSE_INSTALL_BINDIR}/translations COMPONENT glimpse)
endif()

# Qt's deployment (windeployqt on Windows): Qt DLLs, plugins (platform, SVG
# icons, TLS for model downloads), Qt's own translations and the MinGW runtime.
# The editor DLLs are scanned too, so the Qt modules they use come along.
qt_policy(SET QTP0008 NEW) # pass CONTENT through without re-evaluating it
set(_glimpse_deploy_libs "")
foreach(_lib IN LISTS _glimpse_extra_libs)
    string(APPEND _glimpse_deploy_libs " \"${_lib}\"")
endforeach()
if(WIN32)
    set(_glimpse_plugins_dir ".") # plugin folders beside the executable
else()
    set(_glimpse_plugins_dir [=[${QT_DEPLOY_PLUGINS_DIR}]=])
endif()
# Glimpse draws with the raster engine only: skip the 20 MB software OpenGL.
set(_glimpse_deploy_options "")
if(WIN32)
    set(_glimpse_deploy_options "DEPLOY_TOOL_OPTIONS --no-opengl-sw")
endif()
qt_generate_deploy_script(
    TARGET glimpse
    OUTPUT_SCRIPT GLIMPSE_DEPLOY_SCRIPT
    CONTENT "
qt_deploy_runtime_dependencies(
    EXECUTABLE \"${GLIMPSE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:glimpse>\"
    ADDITIONAL_LIBRARIES${_glimpse_deploy_libs}
    BIN_DIR \"${GLIMPSE_INSTALL_BINDIR}\"
    PLUGINS_DIR \"${_glimpse_plugins_dir}\"
    ${_glimpse_deploy_options}
)
")
install(SCRIPT ${GLIMPSE_DEPLOY_SCRIPT} COMPONENT glimpse)

set(CPACK_PACKAGE_NAME "Glimpse")
set(CPACK_PACKAGE_VENDOR "Glimpse")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR ZIP)
set(CPACK_COMPONENTS_ALL glimpse)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
# Unzips into one folder named after the package, not loose files.
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY ON)
if(WIN32)
    set(CPACK_PACKAGE_FILE_NAME "Glimpse-${PROJECT_VERSION}-windows-x64")
else()
    set(CPACK_PACKAGE_FILE_NAME "Glimpse-${PROJECT_VERSION}-linux-${CMAKE_SYSTEM_PROCESSOR}")
endif()
include(CPack)
