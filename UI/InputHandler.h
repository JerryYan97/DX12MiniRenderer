#pragma once
#include <vector>
#include <unordered_map>

enum INPUT_TYPE
{
    KEYBOARD_MOUSE,
    ANIMTION,
    MAX_INPUT_TYPE
};

enum KEYBOARD_MOUSE_INPUT_TYPE
{
    KM_INPUT_KEY_W,
    KM_INPUT_KEY_A,
    KM_INPUT_KEY_S,
    KM_INPUT_KEY_D,
    KM_INPUT_KEY_Q,
    KM_INPUT_KEY_E,
    KM_INPUT_MOUSE_MOVE,
    KM_INPUT_MOUSE_WHEEL,
    MAX_KM_INPUT_TYPE
};

struct StBindingInput
{
    float fVals[4];
    int   iVals[4];
};

typedef void (*PFN_INPUT_BINDING_CALLBACK)(StBindingInput bindingInput);

class InputHandler
{
public:
    InputHandler() {}
    ~InputHandler() {}
    void Init() {}
    void Finalize() {}
    void Tick(float deltaTime);
    void TickKeyboardMouseInputBindings(float deltaTime);

    void BindKeyboardMouseHandler(KEYBOARD_MOUSE_INPUT_TYPE type, PFN_INPUT_BINDING_CALLBACK callback)
    {
        m_keyboardMounseBindingTbl[type] = callback;
    }
private:
    PFN_INPUT_BINDING_CALLBACK m_keyboardMounseBindingTbl[KEYBOARD_MOUSE_INPUT_TYPE::MAX_KM_INPUT_TYPE] = {};
};