# DX12 Mini Renderer

This is a Dx12 renderer practice project to experiment DX12 programming and various real-time rendering techniques. It will keep using original Dx12 API instead of Dx12 wrappers and keep abstractions as less as possible to facilitate referring.

![Front Page](./Gallery/DX12MiniRenderer_FrontPage.png)

![DXR Cornell Boxes](./Gallery/CornellBoxes.png)

![RTMilestone1](./Gallery/RTMilestone1.PNG)

## Current Features
* DXR Path Tracer Backend and Rasterization Forward Backend.
* DXR Path Tracer: Diffuse, Metal, Glass Materials; Area Lights.
* Rasterization Forward Render: GLTF PBR support, Point Lights, etc.
* Base Framework: GLTF Loaderer, Yaml Based Scene Definiation, etc.

## Pull it

```
git clone --recurse-submodules https://github.com/JerryYan97/DX12MiniRenderer.git
```

## Build It
GPU debug events use the [WinPixEventRuntime](https://www.nuget.org/packages/WinPixEventRuntime/) package. Download the pinned version before configuring:
```
SetupWinPixEventRuntime.bat
```

Then configure and build from a Visual Studio developer command prompt:
```
cmake -S . -B build
cmake --build build --config Debug
```

The setup script downloads the package into `Dependencies/WinPixEventRuntime/`, which is ignored by Git. CMake reports a clear error if the package has not been installed.

## Use it

```
C:\DX12MiniRenderer\build\Debug>DX12MiniRenderer.exe -h
  DX12MiniRenderer.exe {OPTIONS}

    DX12 Mini-Renderer. Supports Rasterization/Raytracing Backend and various
    scenes.

  OPTIONS:

      -h, --help                        Display this help menu
      -s, --scene                       The render scene idx.

    E.g. DX12MiniRenderer.exe --scene 1
    (0):Avocado
    (1):CornellBox
    (2):CornellBoxMultiMaterials
    (3):Cube
    (4):Duck
    (5):DXRMilestoneScene
    (6):Fish
    (7):PBRSpheresPtLights
    (8):RoughnessCompareIBL
    (9):Sphere
    (10):TexturedCube
    (11):Triangle
```