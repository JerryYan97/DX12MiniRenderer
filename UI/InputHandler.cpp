#include "InputHandler.h"
#include "imgui.h"

void InputHandler::Tick(float deltaTime)
{
    TickKeyboardMouseInputBindings(deltaTime);
}

void InputHandler::TickKeyboardMouseInputBindings(float deltaTime)
{
    if (ImGui::IsKeyDown(ImGuiKey_W))
    {
        StBindingInput input = {};
        input.fVals[0] = deltaTime;
        m_keyboardMounseBindingTbl[KM_INPUT_KEY_W](input);
    }

    if (ImGui::IsKeyDown(ImGuiKey_A))
    {
        StBindingInput input = {};
        input.fVals[0] = deltaTime;
        m_keyboardMounseBindingTbl[KM_INPUT_KEY_A](input);
    }

    if (ImGui::IsKeyDown(ImGuiKey_S))
    {
        StBindingInput input = {};
        input.fVals[0] = deltaTime;
        m_keyboardMounseBindingTbl[KM_INPUT_KEY_S](input);
    }

    if (ImGui::IsKeyDown(ImGuiKey_D))
    {
        StBindingInput input = {};
        input.fVals[0] = deltaTime;
        m_keyboardMounseBindingTbl[KM_INPUT_KEY_D](input);
    }

    if (ImGui::IsKeyDown(ImGuiKey_Q))
    {
        StBindingInput input = {};
        input.fVals[0] = deltaTime;
        m_keyboardMounseBindingTbl[KM_INPUT_KEY_Q](input);
    }

    if (ImGui::IsKeyDown(ImGuiKey_E))
    {
        StBindingInput input = {};
        input.fVals[0] = deltaTime;
        m_keyboardMounseBindingTbl[KM_INPUT_KEY_E](input);
    }
}