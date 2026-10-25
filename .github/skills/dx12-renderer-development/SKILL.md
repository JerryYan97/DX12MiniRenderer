---
name: dx12-renderer-development
description: Implement a scoped DX12MiniRenderer change from a task description or a repository design document. Use when adding, modifying, debugging, or reviewing renderer, shader, scene, UI, build, or performance work in this repository.
---

# DX12MiniRenderer Development

Use this skill to execute one well-defined change in DX12MiniRenderer. The
request must provide either:

- a task description, ideally including the desired behavior and acceptance
  criteria; or
- a repository-relative path to a design document, task document, issue export,
  or other requirements file.

If both are provided, treat the design document as the source of detailed
requirements and the task description as the requested scope. Read referenced
documents before changing code. If requirements conflict, pause and ask which
source controls the decision. Do not infer substantial product or rendering
design choices that the input leaves open.

## Intake and planning

1. Read `AGENTS.md`, `.github/copilot-instructions.md`, and the supplied
   design document when present.
2. Convert the input into a concise working contract:
   - requested behavior and non-goals;
   - affected user-visible, render, and data-flow behavior;
   - measurable acceptance criteria;
   - likely files and systems to inspect;
   - validation appropriate to the surfaces that change.
3. Inspect existing code and nearby implementations before selecting a design.
   Reuse established resource ownership, descriptor, command-list, shader
   registration, and configuration patterns.
4. Ask for clarification before implementation only when a choice materially
   changes behavior, rendering quality, resource lifetime, or public usage.
   Otherwise make the narrowest reasonable choice and document it in the
   final handoff.

## Implementation rules

- Keep the change focused on the requested task. Do not refactor unrelated
  code or modify `ThirdParty/` unless it is explicitly in scope.
- Preserve the renderer's low-abstraction, explicit DirectX 12 resource
  management style. Initialize GPU and resource structures explicitly.
- Follow local C++20 style, including `m_` member names, existing pointer
  prefixes, and indented declarations and definitions in anonymous namespaces.
- When a feature crosses boundaries, update all required surfaces together:
  C++ code, HLSL, CMake source/shader registration, scene or YAML
  configuration, and assets.
- Maintain DirectX 12 correctness across resource state transitions, descriptor
  heap ownership and lifetime, upload/resource lifetime, synchronization, root
  signatures, and shader binding layouts.
- Retain accurate existing comments. Add comments only where an otherwise
  non-obvious GPU or synchronization decision needs explanation.
- Never add credentials, machine-specific paths, build output, IDE state, or
  generated shader binaries.

## Shader work

For HLSL changes, inspect the matching C++ binding setup and shader compilation
workflow. Keep register spaces, root parameters, constant-buffer packing, input
layouts, and resource formats consistent on both sides. Compile the affected
shader through the repository workflow after setting `DX12_DXC_X64` to the
directory that contains `dxc.exe`, when the environment provides it.

## Validation and handoff

1. Inspect the final diff for scope and integration completeness.
2. Run the narrowest relevant validation:
   - C++ or CMake: configure as needed, then build the
     `DX12MiniRenderer` target in Debug from a Visual Studio developer
     command prompt.
   - HLSL: compile affected shaders using the repository shader workflow.
   - Scene/configuration: validate the edited data against the parser's
     expected format and any available targeted checks.
   - Runtime-rendering behavior: run the application only when a supported
     Windows environment, DirectX 12-capable GPU, and required assets are
     available.
3. State exactly what changed, the validation commands and results, and any
   validation that could not run due to unavailable Windows, DirectX SDK, GPU,
   or DXC prerequisites.

## Prompt examples

```text
Use /dx12-renderer-development to add temporal jitter to the camera projection.
Acceptance criteria: the jitter is applied before TAA, resets after resize, and
does not change the non-TAA path.
```

```text
Use /dx12-renderer-development to implement @docs/designs/gpu-culling.md.
Limit the work to Phase 1 and preserve the current draw path behind the
documented configuration flag.
```
