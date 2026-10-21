cmake_minimum_required(VERSION 3.20)
project(glass_menu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GLASS_MENU_BUILD_DEMO "Build the demo application" ${PROJECT_IS_TOP_LEVEL})
option(GLASS_MENU_BUNDLE_IMGUI "Compile the vendored Dear ImGui into the library" ON)

set(GLASS_MENU_SOURCES
    src/ui_anim.cpp
    src/ui_config.cpp
    src/ui_gfx.cpp
    src/ui_hud.cpp
    src/ui_layout.cpp
    src/ui_overlay.cpp
    src/ui_palette.cpp
    src/ui_registry.cpp
    src/ui_sound.cpp
    src/ui_toast.cpp
    src/lucide_font.cpp
)

if(GLASS_MENU_BUNDLE_IMGUI)
    list(APPEND GLASS_MENU_SOURCES
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_win32.cpp
        imgui/backends/imgui_impl_dx11.cpp
    )
endif()

add_library(glass_menu STATIC ${GLASS_MENU_SOURCES})
add_library(glass_menu::glass_menu ALIAS glass_menu)

target_include_directories(glass_menu PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
)
target_compile_definitions(glass_menu PUBLIC UNICODE _UNICODE)
target_link_libraries(glass_menu PUBLIC d3d11 d3dcompiler dxgi dwmapi winmm comdlg32 ole32 shell32)

if(MSVC)
    target_compile_options(glass_menu PRIVATE /W3 /utf-8)
endif()

if(GLASS_MENU_BUILD_DEMO)
    add_executable(glass_menu_demo WIN32 demo/demo.cpp)
    target_link_libraries(glass_menu_demo PRIVATE glass_menu)
    if(MSVC)
        target_compile_options(glass_menu_demo PRIVATE /utf-8)
    endif()
endif()
