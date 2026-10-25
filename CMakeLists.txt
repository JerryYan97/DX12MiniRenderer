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
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MiniRendererApp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MiniRendererApp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Settings.h)

add_definitions(-DSOURCE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\")

# Category the source files
source_group(ImGUI FILES ${IMGUI_FILES_LIST})
source_group(App FILES ${APP_SRC})

add_subdirectory(./Scene)
add_subdirectory(./UI)
add_subdirectory(./EventSystem)
add_subdirectory(./RenderBackend)
add_subdirectory(./Utils)
add_subdirectory(./TimePerfManager)
add_subdirectory(./RenderBackend/RTShaders)

source_group(Scene FILES ${SCENE_SRC})
source_group(UI FILES ${UI_SRC})
source_group(EventSystem FILES ${EVENTSYSTEM_SRC})
source_group(RenderBackend FILES ${RENDERBACKEND_SRC})
source_group(Utils FILES ${UTILS_SRC})
source_group(TimePerfManager FILES ${TIMEPERF_SRC})
source_group(RTShaders FILES ${RT_SHADERS_SRC})

add_executable(DX12MiniRenderer ${APP_SRC}
                                ${IMGUI_FILES_LIST}
                                ${SCENE_SRC}
                                ${UI_SRC}
                                ${EVENTSYSTEM_SRC}
                                ${RENDERBACKEND_SRC}
                                ${UTILS_SRC}
                                ${TIMEPERF_SRC}
                                ${RT_SHADERS_SRC})

# include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/YamlCpp/include/yaml-cpp)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/YamlCpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/build/yaml)
set_target_properties(yaml-cpp PROPERTIES FOLDER "YamlCpp")
set_target_properties(yaml-cpp-parse PROPERTIES FOLDER "YamlCpp")
set_target_properties(yaml-cpp-read PROPERTIES FOLDER "YamlCpp")
set_target_properties(yaml-cpp-sandbox PROPERTIES FOLDER "YamlCpp")

target_compile_features(DX12MiniRenderer PRIVATE cxx_std_20)

set(PIX_EVENT_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/WinPixEventRuntime")
set(PIX_EVENT_RUNTIME_INCLUDE_DIR "${PIX_EVENT_RUNTIME_ROOT}/Include/WinPixEventRuntime")
set(PIX_EVENT_RUNTIME_LIBRARY "${PIX_EVENT_RUNTIME_ROOT}/bin/x64/WinPixEventRuntime.lib")
set(PIX_EVENT_RUNTIME_DLL "${PIX_EVENT_RUNTIME_ROOT}/bin/x64/WinPixEventRuntime.dll")

if (NOT EXISTS "${PIX_EVENT_RUNTIME_INCLUDE_DIR}/pix3.h" OR
    NOT EXISTS "${PIX_EVENT_RUNTIME_LIBRARY}" OR
    NOT EXISTS "${PIX_EVENT_RUNTIME_DLL}")
    message(FATAL_ERROR
        "WinPixEventRuntime is required. Run SetupWinPixEventRuntime.bat from the repository root.")
endif()

target_include_directories(DX12MiniRenderer PRIVATE "${PIX_EVENT_RUNTIME_INCLUDE_DIR}")
target_link_libraries(DX12MiniRenderer d3d12 dxgi d3dcompiler yaml-cpp "${PIX_EVENT_RUNTIME_LIBRARY}")
add_custom_command(TARGET DX12MiniRenderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PIX_EVENT_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:DX12MiniRenderer>")


# DXC command:
# All raytracing shaders must be compiled as library using lib_6_3/lib_6_4 profile option.
# C:\JiaruiYan\DXCOffical\dxc_2024_07_31\bin\x64>dxc.exe -T lib_6_3 -Fh "C:\JiaruiYan\Projects\DX12MiniRenderer\RenderBackend\RTShaders\RaytracingShader.h" -Fo "C:\JiaruiYan\Projects\DX12MiniRenderer\RenderBackend\RTShaders\RaytracingShader.o" "C:\JiaruiYan\Projects\DX12MiniRenderer\RenderBackend\RTShaders\Raytracing.hlsl"