cmake_minimum_required(VERSION 3.13)
project(DX12MiniRenderer VERSION 0.1 LANGUAGES CXX)

set(DearImGUIPath ./ThirdParty/imgui)
include_directories(${DearImGUIPath})
include_directories(${DearImGUIPath}/backends)

source_group(ImGUI FILES ${DearImGUIPath}/imgui.cpp 
                         ${DearImGUIPath}/imgui_draw.cpp
                         ${DearImGUIPath}/imgui_demo.cpp
                         ${DearImGUIPath}/imgui_tables.cpp
                         ${DearImGUIPath}/imgui_widgets.cpp)

add_executable(DX12MiniRenderer "main.cpp" 
                                ${DearImGUIPath}/imgui.cpp 
                                ${DearImGUIPath}/imgui_draw.cpp
                                ${DearImGUIPath}/imgui_demo.cpp
                                ${DearImGUIPath}/imgui_tables.cpp
                                ${DearImGUIPath}/imgui_widgets.cpp
                                ${DearImGUIPath}/backends/imgui_impl_win32.cpp 
                                ${DearImGUIPath}/backends/imgui_impl_dx12.cpp)

target_compile_features(DX12MiniRenderer PRIVATE cxx_std_17)

target_link_libraries(DX12MiniRenderer d3d12 dxgi d3dcompiler)