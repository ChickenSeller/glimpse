cmake_minimum_required(VERSION 3.21)

project(glimpse VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets Svg LinguistTools)
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

qt_add_executable(glimpse
    src/main.cpp

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
    src/app/Language.cpp
    src/app/Language.h
    src/app/TintedIcon.cpp
    src/app/TintedIcon.h

    src/capture/DesktopSnapshot.cpp
    src/capture/DesktopSnapshot.h
    src/capture/Platform.cpp
    src/capture/Platform.h
    src/capture/QtScreenGrabber.cpp
    src/capture/QtScreenGrabber.h
    src/capture/ScreenGrabber.cpp
    src/capture/ScreenGrabber.h
    src/capture/WindowList.h

    src/hotkey/GlobalHotkeys.h

    src/overlay/RegionSelector.cpp
    src/overlay/RegionSelector.h

    src/editor/EditorWindow.cpp
    src/editor/EditorWindow.h
    src/editor/EditorWindow.ui

    src/settings/HotkeyEdit.cpp
    src/settings/HotkeyEdit.h
    src/settings/SettingsDialog.cpp
    src/settings/SettingsDialog.h
    src/settings/SettingsDialog.ui

    resources/glimpse.qrc
)

target_include_directories(glimpse PRIVATE src)
target_compile_definitions(glimpse PRIVATE GLIMPSE_VERSION="${PROJECT_VERSION}")
# UI translations, compiled to .qm and embedded under :/i18n. Refresh the .ts
# files after changing UI strings with: cmake --build <dir> --target update_translations
qt_add_translations(glimpse
    TS_FILES
        translations/glimpse_zh_CN.ts
        translations/glimpse_ja.ts
)

# Svg: the SVG icon engine plugin that renders the Material Symbols icons.
target_link_libraries(glimpse PRIVATE Qt6::Widgets Qt6::Svg)

# Window/object picking and global hotkeys: Win32 only so far.
if(WIN32)
    target_sources(glimpse PRIVATE
        src/capture/WindowList_win.cpp
        src/hotkey/GlobalHotkeys_win.cpp
    )
    target_link_libraries(glimpse PRIVATE dwmapi)
else()
    target_sources(glimpse PRIVATE
        src/capture/WindowList_stub.cpp
        src/hotkey/GlobalHotkeys_stub.cpp
    )
endif()

# Linux: Wayland screenshots go through xdg-desktop-portal over D-Bus.
if(UNIX AND NOT APPLE)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
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

install(TARGETS glimpse
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
