# Contributing to DX12MiniRenderer

Thank you for contributing to DX12MiniRenderer. This project is a Windows
DirectX 12 renderer using C++20, CMake, HLSL, and Git submodules.

## Getting Started

Clone the repository with its dependencies:

```powershell
git clone --recurse-submodules https://github.com/JerryYan97/DX12MiniRenderer.git
cd DX12MiniRenderer
```

Use a Visual Studio developer command prompt to configure and build:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Run the application:

```powershell
.\build\Debug\DX12MiniRenderer.exe --help
```

Shader compilation requires DXC. Set `DX12_DXC_X64` to the directory containing
`dxc.exe` before using the shader compilation workflow.

## Making Changes

1. Create a focused branch from the current default branch.
2. Read [AGENTS.md](AGENTS.md) for project conventions and AI-agent guidance.
3. Keep the change limited to its purpose and follow the style of nearby code.
4. Do not modify code under `ThirdParty/` unless the change specifically
   requires a dependency update.
5. Build the affected target and perform the most relevant available validation.
6. Commit with a short, imperative description of the change.

## Pull Requests

Open one pull request per cohesive change. Complete the pull request template,
link related issues, explain the effect of the change, and include the commands
or manual checks used to validate it. Call out any validation that could not run
because it requires specific Windows, DirectX, GPU, or DXC tooling.

## Reporting Issues

Include the renderer configuration, scene, GPU and driver details, reproduction
steps, expected behavior, actual behavior, and relevant logs or screenshots.

## AI-Assisted Contributions

AI-assisted changes are welcome when they meet the same quality bar as any
other contribution. Review generated code carefully, preserve project
conventions, and follow [AGENTS.md](AGENTS.md).
