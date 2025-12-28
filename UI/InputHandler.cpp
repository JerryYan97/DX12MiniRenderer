#include "InputHandler.h"
#include "imgui.h"
#include "../Utils/crc32.h"
#include "../EventSystem/EventManager.h"

void InputHandler::Tick(float deltaTime)
{
    TickKeyboardMouseInputBindings(deltaTime);
}

void InputHandler::TickKeyboardMouseInputBindings(float deltaTime)
{
    HEventManager* pEventManager = HEventManager::HEventManagerInstance();

    if (ImGui::IsKeyDown(ImGuiKey_W))
    {
        HEventArguments args;
        args[crc32("delta")] = deltaTime;
        HEvent event(args, "MoveForward");
        pEventManager->SendEvent(event);
    }

    if (ImGui::IsKeyDown(ImGuiKey_A))
    {
        HEventArguments args;
        args[crc32("delta")] = deltaTime;
        HEvent event(args, "MoveLeft");
        pEventManager->SendEvent(event);
    }

    if (ImGui::IsKeyDown(ImGuiKey_S))
    {
        HEventArguments args;
        args[crc32("delta")] = deltaTime;
        HEvent event(args, "MoveBackward");
        pEventManager->SendEvent(event);
    }

    if (ImGui::IsKeyDown(ImGuiKey_D))
    {
        HEventArguments args;
        args[crc32("delta")] = deltaTime;
        HEvent event(args, "MoveRight");
        pEventManager->SendEvent(event);
    }

    if (ImGui::IsKeyDown(ImGuiKey_Q))
    {
        HEventArguments args;
        args[crc32("delta")] = deltaTime;
        HEvent event(args, "MoveDown");
        pEventManager->SendEvent(event);
    }

    if (ImGui::IsKeyDown(ImGuiKey_E))
    {
        HEventArguments args;
        args[crc32("delta")] = deltaTime;
        HEvent event(args, "MoveUp");
        pEventManager->SendEvent(event);
    }

    // Z: Zoom In, X: Zoom Out.
    if (ImGui::IsKeyDown(ImGuiKey_Z))
    {
        HEventArguments args;
        args[crc32("delta")] = deltaTime;
        HEvent event(args, "ZoomCamera");
        pEventManager->SendEvent(event);
    }

    if (ImGui::IsKeyDown(ImGuiKey_X))
    {
        HEventArguments args;
        args[crc32("delta")] = -deltaTime;
        HEvent event(args, "ZoomCamera");
        pEventManager->SendEvent(event);
    }
}

