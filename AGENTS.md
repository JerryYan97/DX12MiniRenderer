# AI Agent Contribution Guide

This file contains the shared instructions for AI agents working in
DX12MiniRenderer. Copilot-specific instructions are in
`.github/copilot-instructions.md`.

## Project Context

DX12MiniRenderer is a Windows DirectX 12 renderer written in C++20. It uses
CMake, DirectX 12, DXC-compiled HLSL shaders, YAML scene definitions, and
third-party dependencies checked out as Git submodules.

Key directories:

- `RenderBackend/`: rendering implementations and shaders.
- `Scene/`, `UI/`, `EventSystem/`, `Utils/`, `TimePerfManager/`: engine and
  application subsystems.
- `Assets/` and `SceneConfig.yaml`: sample assets and scene configuration.
- `ThirdParty/`: external dependencies. Do not modify vendor code unless the
  task explicitly requires it.

## Setup and Validation

Clone dependencies before configuring the project:

```powershell
git clone --recurse-submodules https://github.com/JerryYan97/DX12MiniRenderer.git
```

Configure and build with a Visual Studio developer command prompt:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Run the application from the build output directory:

```powershell
.\build\Debug\DX12MiniRenderer.exe --help
```

Use the narrowest applicable validation after making a change. For C++ or CMake
changes, build the `DX12MiniRenderer` target. For HLSL changes, compile the
affected shader with the repository shader workflow after setting
`DX12_DXC_X64` to the directory containing `dxc.exe`.

## Implementation Rules

- Read the relevant code and surrounding patterns before editing.
- Keep changes focused on the requested behavior. Do not perform unrelated
  refactors or formatting churn.
- Match the existing local style. Use `m_` for members and retain existing
  pointer-prefix conventions.
- Prefer explicit initialization for GPU and resource structures.
- Keep functions practical and compact. Do not introduce template-heavy
  abstractions without a clear need.
- Preserve comments unless they are inaccurate or no longer describe the code.
- Keep source, CMake target lists, shader registration, scene configuration,
  and assets consistent when a change spans those surfaces.
- Add or update validation and documentation when behavior or public usage
  changes.

## Safety and Repository Hygiene

- Never add credentials, API keys, tokens, or machine-specific secrets.
- Do not commit generated build output, IDE state, binaries, shader objects, or
  other files ignored by `.gitignore`.
- Do not overwrite, revert, or discard changes made by others.
- Treat `ThirdParty/` as externally maintained code and do not reformat it.
- Do not rewrite published Git history or force-push unless explicitly asked.

## GitHub Workflow

- Work on a focused branch and make logically scoped commits with imperative,
  descriptive messages.
- Before opening a pull request, inspect the diff and run the relevant
  validation where the local Windows and DirectX environment permits it.
- In pull requests, describe the user-visible or technical change, list the
  validation performed, and call out known limitations or follow-up work.
- Use the pull request template checklist and link related GitHub issues when
  applicable.
