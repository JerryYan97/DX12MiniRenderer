cmake_minimum_required(VERSION 3.13)
project(DX12MiniRenderer VERSION 0.1 LANGUAGES CXX)

set(DearImGUIPath ./ThirdParty/imgui)
include_directories(${DearImGUIPath})
include_directories(${DearImGUIPath}/backends)

set(IMGUI_FILES_LIST ${DearImGUIPath}/imgui.cpp 
                     ${DearImGUIPath}/imgui_draw.cpp
                     ${DearImGUIPath}/imgui_demo.cpp
                     ${DearImGUIPath}/imgui_tables.cpp
                     ${DearImGUIPath}/imgui_widgets.cpp
                     ${DearImGUIPath}/backends/imgui_impl_win32.cpp 
                     ${DearImGUIPath}/backends/imgui_impl_dx12.cpp)

# Category the source files
source_group(ImGUI FILES ${IMGUI_FILES_LIST})

add_executable(DX12MiniRenderer "main.cpp"
                                "MiniRendererApp.h"
                                "MiniRendererApp.cpp"
                                ${IMGUI_FILES_LIST})

add_subdirectory(./Scene)
add_subdirectory(./UI)
add_subdirectory(./EventSystem)
add_subdirectory(./RenderBackend)
add_subdirectory(./Utils)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/YamlCpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/build/yaml)
set_target_properties(yaml-cpp PROPERTIES FOLDER "YamlCpp")
set_target_properties(yaml-cpp-parse PROPERTIES FOLDER "YamlCpp")
set_target_properties(yaml-cpp-read PROPERTIES FOLDER "YamlCpp")
set_target_properties(yaml-cpp-sandbox PROPERTIES FOLDER "YamlCpp")

target_compile_features(DX12MiniRenderer PRIVATE cxx_std_17)

target_link_libraries(DX12MiniRenderer d3d12 dxgi d3dcompiler)