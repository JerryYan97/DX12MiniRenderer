cmake_minimum_required(VERSION 3.13)
project(DX12MiniRenderer VERSION 0.1 LANGUAGES CXX)

set(DearImGUIPath ./ThirdParty/imgui)
include_directories(${DearImGUIPath}
                    ${DearImGUIPath}/backends
                    ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/TinyGltf
                    ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/arg)

set(IMGUI_FILES_LIST ${DearImGUIPath}/imgui.cpp 
                     ${DearImGUIPath}/imgui_draw.cpp
                     ${DearImGUIPath}/imgui_demo.cpp
                     ${DearImGUIPath}/imgui_tables.cpp
                     ${DearImGUIPath}/imgui_widgets.cpp
                     ${DearImGUIPath}/backends/imgui_impl_win32.cpp 
                     ${DearImGUIPath}/backends/imgui_impl_dx12.cpp)

set(APP_SRC
    main.cpp
    MiniRendererApp.h
    MiniRendererApp.cpp)

add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")

# Category the source files
source_group(ImGUI FILES ${IMGUI_FILES_LIST})
source_group(App FILES ${APP_SRC})

add_executable(DX12MiniRenderer ${APP_SRC}
                                ${IMGUI_FILES_LIST})

add_subdirectory(./Scene)
add_subdirectory(./UI)
add_subdirectory(./EventSystem)
add_subdirectory(./RenderBackend)
add_subdirectory(./Utils)
add_subdirectory(./TimePerfManager)

# include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/YamlCpp/include/yaml-cpp)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/YamlCpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/build/yaml)
set_target_properties(yaml-cpp PROPERTIES FOLDER "YamlCpp")
set_target_properties(yaml-cpp-parse PROPERTIES FOLDER "YamlCpp")
set_target_properties(yaml-cpp-read PROPERTIES FOLDER "YamlCpp")
set_target_properties(yaml-cpp-sandbox PROPERTIES FOLDER "YamlCpp")

target_compile_features(DX12MiniRenderer PRIVATE cxx_std_20)

target_link_libraries(DX12MiniRenderer d3d12 dxgi d3dcompiler yaml-cpp)


# DXC command:
# All raytracing shaders must be compiled as library using lib_6_3/lib_6_4 profile option.
# C:\JiaruiYan\DXCOffical\dxc_2024_07_31\bin\x64>dxc.exe -T lib_6_3 -Fh "C:\JiaruiYan\Projects\DX12MiniRenderer\RenderBackend\RTShaders\RaytracingShader.h" -Fo "C:\JiaruiYan\Projects\DX12MiniRenderer\RenderBackend\RTShaders\RaytracingShader.o" "C:\JiaruiYan\Projects\DX12MiniRenderer\RenderBackend\RTShaders\Raytracing.hlsl"