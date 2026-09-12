# Copilot Instructions

Follow the repository-wide guidance in [AGENTS.md](../AGENTS.md), which is the
source of truth for implementation practices, validation, safety, and GitHub
workflow.

For this DirectX 12 C++20 renderer:

- Preserve the existing low-abstraction, explicit resource-management approach.
- Treat `ThirdParty/` as vendor code unless a task explicitly targets it.
- Make HLSL, C++, CMake, and scene/configuration updates together when they are
  required for a complete feature.
- Do not assume the build or runtime can be validated outside a Windows
  environment with the DirectX SDK, a supported GPU, and DXC where applicable.
