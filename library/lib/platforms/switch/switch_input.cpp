/*
    Copyright 2021 natinusala

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <chrono>
#include <borealis/core/thread.hpp>
#include <borealis/core/application.hpp>
#include <borealis/platforms/switch/switch_input.hpp>
#include <cstdarg>
#include <cstdio>
#ifdef __SWITCH2__
#include <borealis/platforms/switch/switch_touch_coordinates.hpp>
#include <borealis/platforms/switch/switch_chat_button.hpp>
#endif
#include <borealis/core/application.hpp>

// Avoid namespace collision
#define NXEvent ::Event

using namespace std::chrono_literals;

namespace brls
{

static const uint64_t HidNpadButton_None = ((uint64_t)1 << (63));

static const uint64_t SWITCH_BUTTONS_FULL_MAPPING[_BUTTON_MAX] = {
    HidNpadButton_ZL, // BUTTON_LT
    HidNpadButton_L, // BUTTON_LB

    HidNpadButton_StickL, // BUTTON_LSB

    HidNpadButton_Up, // BUTTON_UP
    HidNpadButton_Right, // BUTTON_RIGHT
    HidNpadButton_Down, // BUTTON_DOWN
    HidNpadButton_Left, // BUTTON_LEFT

    HidNpadButton_Minus, // BUTTON_BACK
    HidNpadButton_None, // BUTTON_GUIDE
    HidNpadButton_Plus, // BUTTON_START

    HidNpadButton_StickR, // BUTTON_RSB

    HidNpadButton_Y, // BUTTON_Y
    HidNpadButton_B, // BUTTON_B
    HidNpadButton_A, // BUTTON_A
    HidNpadButton_X, // BUTTON_X

    HidNpadButton_R, // BUTTON_RB
    HidNpadButton_ZR, // BUTTON_RT

    HidNpadButton_AnyUp, // BUTTON_NAV_UP
    HidNpadButton_AnyRight, // BUTTON_NAV_RIGHT
    HidNpadButton_AnyDown, // BUTTON_NAV_DOWN
    HidNpadButton_AnyLeft, // BUTTON_NAV_LEFT
};

static const uint64_t SWITCH_BUTTONS_HALF_MAPPING[_BUTTON_MAX] = {
    HidNpadButton_None, // BUTTON_LT
    HidNpadButton_AnySL, // BUTTON_LB

    HidNpadButton_StickL | HidNpadButton_StickR, // BUTTON_LSB

    // HidNpadButton_StickLRight | HidNpadButton_StickRLeft, // BUTTON_UP
    // HidNpadButton_StickLDown  | HidNpadButton_StickRUp, // BUTTON_RIGHT
    // HidNpadButton_StickLLeft  | HidNpadButton_StickRRight, // BUTTON_DOWN
    // HidNpadButton_StickLUp    | HidNpadButton_StickRDown, // BUTTON_LEFT

    HidNpadButton_None, // BUTTON_UP
    HidNpadButton_None, // BUTTON_RIGHT
    HidNpadButton_None, // BUTTON_DOWN
    HidNpadButton_None, // BUTTON_LEFT

    HidNpadButton_None, // BUTTON_BACK
    HidNpadButton_None, // BUTTON_GUIDE
    HidNpadButton_Plus | HidNpadButton_Minus, // BUTTON_START

    HidNpadButton_None, // BUTTON_RSB

    HidNpadButton_B | HidNpadButton_Up, // BUTTON_Y
    HidNpadButton_A | HidNpadButton_Left, // BUTTON_B
    HidNpadButton_X | HidNpadButton_Down, // BUTTON_A
    HidNpadButton_Y | HidNpadButton_Right, // BUTTON_X

    HidNpadButton_AnySR, // BUTTON_RB
    HidNpadButton_None, // BUTTON_RT

    HidNpadButton_StickLRight | HidNpadButton_StickRLeft, // BUTTON_NAV_UP
    HidNpadButton_StickLDown | HidNpadButton_StickRUp, // BUTTON_NAV_RIGHT
    HidNpadButton_StickLLeft | HidNpadButton_StickRRight, // BUTTON_NAV_DOWN
    HidNpadButton_StickLUp | HidNpadButton_StickRDown, // BUTTON_NAV_LEFT
};

static const size_t SWITCH_AXIS_MAPPING[_AXES_MAX] = {
    LEFT_X,
    LEFT_Y,
    RIGHT_X,
    RIGHT_Y,
};

SwitchInputManager::SwitchInputManager()
{
    this->system_button_thread = std::jthread(&SwitchInputManager::system_button_thread_fn, this);

    padConfigureInput(GAMEPADS_MAX, HidNpadStyleSet_NpadStandard);
    hidSetNpadJoyHoldType(HidNpadJoyHoldType_Horizontal);
    hidSetNpadHandheldActivationMode(HidNpadHandheldActivationMode_Single);

    padInitialize(&this->padStateHandheld, HidNpadIdType_Handheld);
    hidInitializeVibrationDevices(m_vibration_device_handheld, 2, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
    padUpdate(&this->padStateHandheld);

    for (int i = 0; i < GAMEPADS_MAX; i++)
    {
        padInitialize(&this->padsState[i], (HidNpadIdType)i);
        padUpdate(&this->padsState[i]);
        padsStyleSet[i] = this->padsState[i].style_set;
        clearVibration(i);
    }

    const Result mouseResult = this->mouse.initialize();
#ifdef __SWITCH2__
    if (R_FAILED(mouseResult))
        Logger::warning("SWITCH2_JOYCON_MOUSE: initialization failed rc=0x{:x}; USB mouse remains available", mouseResult);
    else
        Logger::info("SWITCH2_JOYCON_MOUSE: initialized; L/R=left ZL/ZR=right stick=middle");
#else
    (void)mouseResult;
#endif
    hidInitializeKeyboard();
    hidInitializeKeyboardMap();

    m_hid_keyboard_state.assign(256, false);

    // Handheld has its own sixaxis handle; per-pad handles are acquired lazily in refreshPadSixAxis().
    Result handheldHandleResult = hidGetSixAxisSensorHandles(
        &this->m_six_axis_sensor_handle_handheld, 1, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
    if (R_FAILED(handheldHandleResult))
        Logger::warning("SIXAXIS: handheld handle acquisition failed rc=0x{:x}", handheldHandleResult);
    else
    {
        Result handheldStartResult = hidStartSixAxisSensor(this->m_six_axis_sensor_handle_handheld);
        m_six_axis_sensor_handheld_active = R_SUCCEEDED(handheldStartResult);
        if (R_FAILED(handheldStartResult))
            Logger::warning("SIXAXIS: handheld start failed rc=0x{:x}", handheldStartResult);
    }

    for (auto& sixAxis : this->m_pad_six_axis)
        sixAxis = PadSixAxis{ };
}

SwitchInputManager::~SwitchInputManager()
{
    this->system_button_thread.request_stop();
    if (this->system_button_thread.joinable())
        this->system_button_thread.join();

    if (this->cursorTexture != 0)
    {
        NVGcontext* vg = Application::getNVGContext();
        nvgDeleteImage(vg, this->cursorTexture);
        this->cursorTexture = 0;
    }

    if (m_six_axis_sensor_handheld_active)
        hidStopSixAxisSensor(this->m_six_axis_sensor_handle_handheld);
    for (auto& sixAxis : this->m_pad_six_axis)
        for (int i = 0; i < sixAxis.count; i++)
            hidStopSixAxisSensor(sixAxis.handles[i]);

}

void SwitchInputManager::clearVibration(int controller)
{
    Logger::debug("Vibration clear #{}", controller);
    hidInitializeVibrationDevices(m_vibration_device_handles[controller], 2, (HidNpadIdType)controller, HidNpadStyleTag_NpadJoyDual);
    sendRumbleInternal(m_vibration_device_handles[controller], m_vibration_values[controller], 160.0f, 320.0f, 0.0f, 0.0f);
}

void SwitchInputManager::updateUnifiedControllerState(ControllerState* state)
{
    for (size_t i         = 0; i < _BUTTON_MAX; i++)
        state->buttons[i] = false;

    for (size_t i      = 0; i < _AXES_MAX; i++)
        state->axes[i] = 0;

    auto connected = getControllersConnectedCount();
    for (int i = 0; i < connected; i++)
    {
        ControllerState localState;
        updateControllerState(&localState, i);
        for (size_t j = 0; j < _BUTTON_MAX; j++)
            state->buttons[j] |= localState.buttons[j];

        for (size_t j = 0; j < _AXES_MAX; j++)
        {
            state->axes[j] += localState.axes[j];

            if (state->axes[j] < -1)
                state->axes[j] = -1;
            else if (state->axes[j] > 1)
                state->axes[j] = 1;
        }
    }

    state->buttons[BUTTON_NAV_UP] |= getKeyboardKeyState(BRLS_KBD_KEY_UP);
    state->buttons[BUTTON_NAV_RIGHT] |= getKeyboardKeyState(BRLS_KBD_KEY_RIGHT);
    state->buttons[BUTTON_NAV_DOWN] |= getKeyboardKeyState(BRLS_KBD_KEY_DOWN);
    state->buttons[BUTTON_NAV_LEFT] |= getKeyboardKeyState(BRLS_KBD_KEY_LEFT);

    if (Application::isSwapInputKeys())
    {
        state->buttons[BUTTON_B] |= getKeyboardKeyState(BRLS_KBD_KEY_ENTER);
        state->buttons[BUTTON_A] |= getKeyboardKeyState(BRLS_KBD_KEY_ESCAPE);
    }
    else
    {
        state->buttons[BUTTON_A] |= getKeyboardKeyState(BRLS_KBD_KEY_ENTER);
        state->buttons[BUTTON_B] |= getKeyboardKeyState(BRLS_KBD_KEY_ESCAPE);
    }
}

short SwitchInputManager::getControllersConnectedCount()
{
    padUpdate(&this->padStateHandheld);
    int controllers = padStateHandheld.active_handheld ? 1 : 0;
    for (int i = 0; i < GAMEPADS_MAX && controllers < GAMEPADS_MAX; i++)
    {
        padUpdate(&this->padsState[i]);
        if (padsState[i].style_set == 0)
            break;

        if (padsState[i].style_set & HidNpadStyleTag_NpadJoyDual)
        {
            const u64 attrib = padGetAttributes(&padsState[i]);
            controllers += (attrib & HidNpadAttribute_IsLeftConnected) ? 1 : 0;
            controllers += (attrib & HidNpadAttribute_IsRightConnected) ? 1 : 0;
        }
        else
            controllers++;
    }
    return controllers > GAMEPADS_MAX ? GAMEPADS_MAX : controllers;
}

bool SwitchInputManager::resolveController(int controller, PadState** pad, int* padIndex, HalfJoyConSide* side)
{
    if (controller < 0)
        return false;

    padUpdate(&this->padStateHandheld);
    int logicalIndex = 0;
    if (padStateHandheld.active_handheld)
    {
        if (controller == logicalIndex)
        {
            *pad = &padStateHandheld;
            *padIndex = -1;
            *side = HalfJoyConSide::NONE;
            return true;
        }
        logicalIndex++;
    }

    for (int i = 0; i < GAMEPADS_MAX && logicalIndex < GAMEPADS_MAX; i++)
    {
        padUpdate(&padsState[i]);
        if (padsState[i].style_set == 0)
            break;

        if (padsState[i].style_set & HidNpadStyleTag_NpadJoyDual)
        {
            const u64 attrib = padGetAttributes(&padsState[i]);
            if (attrib & HidNpadAttribute_IsLeftConnected)
            {
                if (controller == logicalIndex)
                {
                    *pad = &padsState[i];
                    *padIndex = i;
                    *side = HalfJoyConSide::LEFT;
                    return true;
                }
                logicalIndex++;
            }
            if (attrib & HidNpadAttribute_IsRightConnected)
            {
                if (controller == logicalIndex)
                {
                    *pad = &padsState[i];
                    *padIndex = i;
                    *side = HalfJoyConSide::RIGHT;
                    return true;
                }
                logicalIndex++;
            }
        }
        else
        {
            if (controller == logicalIndex)
            {
                *pad = &padsState[i];
                *padIndex = i;
                *side = halfJoyConSide(padsState[i].style_set);
                return true;
            }
            logicalIndex++;
        }
    }
    return false;
}

void SwitchInputManager::updateControllerState(ControllerState* state, int controller)
{
    *state = { };
    PadState* pad = nullptr;
    int padIndex = -1;
    HalfJoyConSide side = HalfJoyConSide::NONE;
    if (!resolveController(controller, &pad, &padIndex, &side))
        return;

    updateControllerStateInner(state, pad, side, padIndex);

    if (padIndex >= 0 && padsStyleSet[padIndex] != pad->style_set)
    {
        padsStyleSet[padIndex] = pad->style_set;
        clearVibration(padIndex);
    }
}

void SwitchInputManager::updateControllerStateInner(ControllerState* state, PadState* pad, HalfJoyConSide logicalSide, int padIndex)
{
    padUpdate(pad);
    uint64_t keysDown = padGetButtons(pad);
#ifdef __SWITCH2__
    const auto mouseSide = this->mouse.mouseSide(pad);
    if (mouseSide == PadPointerSide_Left)
        keysDown &= ~(HidNpadButton_L | HidNpadButton_ZL | HidNpadButton_StickL
            | HidNpadButton_StickLLeft | HidNpadButton_StickLRight | HidNpadButton_StickLUp | HidNpadButton_StickLDown);
    else if (mouseSide == PadPointerSide_Right)
        keysDown &= ~(HidNpadButton_R | HidNpadButton_ZR | HidNpadButton_StickR
            | HidNpadButton_StickRLeft | HidNpadButton_StickRRight | HidNpadButton_StickRUp | HidNpadButton_StickRDown);
#endif

    const bool joyDualLogical = (pad->style_set & HidNpadStyleTag_NpadJoyDual)
        && logicalSide != HalfJoyConSide::NONE;
    if (joyDualLogical)
    {
        const int sideIndex = logicalSide == HalfJoyConSide::LEFT ? 0 : 1;
        int owner = -1;
        for (int i = 0; i < GAMEPADS_MAX; ++i)
            if (pad->id_mask & (1U << i))
            {
                owner = i;
                break;
            }

        const int system = logicalSide == HalfJoyConSide::LEFT ? 0 : 1;
        const bool held = system == 0 ? m_isScreenshotPressed.load() : m_isHomePressed.load();
        const bool systemPressed = halfMappingEnabled && held && halfSystemConsumed[system]
            && systemButtonOwners[system] == owner;
        bool chatPressed = false;
#ifdef __SWITCH2__
        chatPressed = halfMappingEnabled && logicalSide == HalfJoyConSide::RIGHT && m_isChatPressed
            && halfSystemConsumed[2] && systemButtonOwners[2] == owner;
#endif
        const HalfJoyConMapping mapping = halfMappingEnabled
            ? halfMappings[sideIndex]
            : defaultHalfJoyConMapping(logicalSide);
        applyHalfJoyConMapping(state, mapping, logicalSide, keysDown, systemPressed, chatPressed);

        HidAnalogStickState stick = padGetStickPos(pad, sideIndex);
#ifdef __SWITCH2__
        if ((logicalSide == HalfJoyConSide::LEFT && mouseSide == PadPointerSide_Left)
            || (logicalSide == HalfJoyConSide::RIGHT && mouseSide == PadPointerSide_Right))
            stick = { };
#endif
        const float stickX = static_cast<float>(logicalSide == HalfJoyConSide::LEFT ? -stick.y : stick.y) / 32767.0f;
        const float stickY = static_cast<float>(logicalSide == HalfJoyConSide::LEFT ? -stick.x : stick.x) / 32767.0f;
        state->axes[LEFT_X] = stickX;
        state->axes[LEFT_Y] = stickY;
        state->axes[RIGHT_X] = 0;
        state->axes[RIGHT_Y] = 0;
        state->axes[LEFT_Z] = 0;
        state->axes[RIGHT_Z] = 0;
        (void)padIndex;
        return;
    }

    bool full = pad->style_set & HidNpadStyleSet_NpadFullCtrl;

    for (size_t i = 0; i < _BUTTON_MAX; i++)
    {
        uint64_t switchKey = full ? SWITCH_BUTTONS_FULL_MAPPING[i] : SWITCH_BUTTONS_HALF_MAPPING[i];
        state->buttons[i]  = keysDown & switchKey;
    }

    state->buttons[BUTTON_GUIDE] = false;

    if (m_screenshotButtonMode == ButtonOverrideMode::GUIDE_BUTTON)
        state->buttons[BUTTON_GUIDE] |= isScreenshotButtonPressed();

    if (m_homeButtonMode == ButtonOverrideMode::GUIDE_BUTTON)
        state->buttons[BUTTON_GUIDE] |= isHomeButtonPressed();
#ifdef __SWITCH2__
    if (m_chatButtonMode == ButtonOverrideMode::GUIDE_BUTTON)
        state->buttons[BUTTON_GUIDE] |= isChatButtonPressed();
#endif

    applyHalfMapping(state, pad, keysDown);

    HidAnalogStickState analog_stick_l = padGetStickPos(pad, 0);
    HidAnalogStickState analog_stick_r = padGetStickPos(pad, 1);
#ifdef __SWITCH2__
    if (mouseSide == PadPointerSide_Left)
        analog_stick_l = {};
    else if (mouseSide == PadPointerSide_Right)
        analog_stick_r = {};
#endif

    if (full)
    {
        state->axes[LEFT_X]  = (float)analog_stick_l.x / (float)0x7FFF;
        state->axes[LEFT_Y]  = (float)analog_stick_l.y / (float)0x7FFF * -1.0f;
        state->axes[RIGHT_X] = (float)analog_stick_r.x / (float)0x7FFF;
        state->axes[RIGHT_Y] = (float)analog_stick_r.y / (float)0x7FFF * -1.0f;
    }
    else
    {
        auto swap = Application::isSwapHalfJoyconStickToDpad();
        auto x = (float)analog_stick_l.y / (float)0x7FFF * -1.0f + (float)analog_stick_r.y / (float)0x7FFF;
        auto y = (float)analog_stick_l.x / (float)0x7FFF * -1.0f + (float)analog_stick_r.x / (float)0x7FFF;
           
        if (!swap) {
            state->axes[LEFT_X] = x;
            state->axes[LEFT_Y] = y;
        } else {
            state->buttons[BUTTON_UP] = y < -0.3f;
            state->buttons[BUTTON_DOWN] = y > 0.3f;
            state->buttons[BUTTON_RIGHT] = x > 0.3f;
            state->buttons[BUTTON_LEFT] = x < -0.3f;
        }
        state->axes[RIGHT_X] = 0;
        state->axes[RIGHT_Y] = 0;
    }

    state->axes[LEFT_Z]  = 0; // SWITCH NOT SUPPORT ZL AXIS
    state->axes[RIGHT_Z] = 0; // SWITCH NOT SUPPORT ZR AXIS
}

bool SwitchInputManager::getKeyboardKeyState(BrlsKeyboardScancode key)
{
    return m_hid_keyboard_state[m_brls_hid_map[key]];
}

void SwitchInputManager::updateTouchStates(std::vector<RawTouchState>* states)
{
    // Get touchscreen state
    static HidTouchScreenState hidState;

    if (hidGetTouchScreenStates(&hidState, 1))
    {
        for (int i = 0; i < hidState.count; i++)
        {
            RawTouchState state;
            state.pressed    = true;
            state.fingerId   = hidState.touches[i].finger_id;
#ifdef __SWITCH2__
            state.position.x = switchTouchCoordinate(hidState.touches[i].x, 1280.0f,
                Application::windowWidth, Application::windowScale);
            state.position.y = switchTouchCoordinate(hidState.touches[i].y, 720.0f,
                Application::windowHeight, Application::windowScale);
#else
            state.position.x = hidState.touches[i].x / Application::windowScale;
            state.position.y = hidState.touches[i].y / Application::windowScale;
#endif
            states->push_back(state);
        }
    }
}

void SwitchInputManager::sendRumbleInternal(HidVibrationDeviceHandle vibration_device[2], HidVibrationValue vibration_values[2], unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
     vibration_values[0].amp_low = 
    vibration_values[0].amp_high = lowFreqMotor == 0 ? 0.0f : 320.0f;
    vibration_values[1].amp_low = 
    vibration_values[1].amp_high = highFreqMotor == 0 ? 0.0f : 320.0f;

    vibration_values[0].freq_low = 
    vibration_values[0].freq_high = lowFreqMotor == 0 ? 160.0f : (float) lowFreqMotor / 204;

    vibration_values[1].freq_low = highFreqMotor == 0 ? 160.0f : (float) highFreqMotor / 204;
    vibration_values[1].freq_high = highFreqMotor == 0 ? 160.0f : (float) highFreqMotor / 204;

    hidSendVibrationValues(vibration_device, vibration_values, 2);
}

void SwitchInputManager::sendRumbleInternal(HidVibrationDeviceHandle vibration_device[2], HidVibrationValue vibration_values[2], unsigned short lowFreqMotor, unsigned short highFreqMotor, unsigned short leftTriggerFreqMotor, unsigned short rightTriggerFreqMotor)
{
    vibration_values[0].amp_low = lowFreqMotor == 0 ? 0.0f : 320.0f;
    vibration_values[0].amp_high = leftTriggerFreqMotor == 0 ? 0.0f : 320.0f;
    vibration_values[1].amp_low = highFreqMotor == 0 ? 0.0f : 320.0f;
    vibration_values[1].amp_high = rightTriggerFreqMotor == 0 ? 0.0f : 320.0f;

    vibration_values[0].freq_low = lowFreqMotor == 0 ? 160.0f : (float) lowFreqMotor / 204;
    vibration_values[1].freq_low = highFreqMotor == 0 ? 160.0f : (float) highFreqMotor / 204;

    vibration_values[0].freq_high = leftTriggerFreqMotor == 0 ? 320.0f : (float) leftTriggerFreqMotor / 204;
    vibration_values[1].freq_high = rightTriggerFreqMotor == 0 ? 320.0f : (float) rightTriggerFreqMotor / 204;

    hidSendVibrationValues(vibration_device, vibration_values, 2);
}

void SwitchInputManager::sendRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
    PadState* pad = nullptr;
    int padIndex = -1;
    HalfJoyConSide side = HalfJoyConSide::NONE;
    if (!resolveController(controller, &pad, &padIndex, &side))
        return;

    if (padIndex < 0)
    {
        sendRumbleInternal(m_vibration_device_handheld, m_vibration_values_handheld, lowFreqMotor, highFreqMotor);
        return;
    }

    if ((pad->style_set & HidNpadStyleTag_NpadJoyDual) && side != HalfJoyConSide::NONE)
    {
        const int handleIndex = side == HalfJoyConSide::LEFT ? 0 : 1;
        const unsigned short motor = side == HalfJoyConSide::LEFT ? lowFreqMotor : highFreqMotor;
        HidVibrationValue value = { 0 };
        value.amp_low = value.amp_high = motor == 0 ? 0.0f : 320.0f;
        value.freq_low = value.freq_high = motor == 0 ? 160.0f : static_cast<float>(motor) / 204.0f;
        hidSendVibrationValues(&m_vibration_device_handles[padIndex][handleIndex], &value, 1);
        return;
    }

    sendRumbleInternal(m_vibration_device_handles[padIndex], m_vibration_values[padIndex], lowFreqMotor, highFreqMotor);
}

void SwitchInputManager::sendRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor, unsigned short leftTriggerFreqMotor, unsigned short rightTriggerFreqMotor)
{
    PadState* pad = nullptr;
    int padIndex = -1;
    HalfJoyConSide side = HalfJoyConSide::NONE;
    if (!resolveController(controller, &pad, &padIndex, &side))
        return;

    if (padIndex < 0)
    {
        sendRumbleInternal(m_vibration_device_handheld, m_vibration_values_handheld, lowFreqMotor, highFreqMotor, leftTriggerFreqMotor, rightTriggerFreqMotor);
        return;
    }

    if ((pad->style_set & HidNpadStyleTag_NpadJoyDual) && side != HalfJoyConSide::NONE)
    {
        const int handleIndex = side == HalfJoyConSide::LEFT ? 0 : 1;
        const unsigned short motor = side == HalfJoyConSide::LEFT ? lowFreqMotor : highFreqMotor;
        const unsigned short triggerMotor = side == HalfJoyConSide::LEFT ? leftTriggerFreqMotor : rightTriggerFreqMotor;
        HidVibrationValue value = { 0 };
        value.amp_low = motor == 0 ? 0.0f : 320.0f;
        value.amp_high = triggerMotor == 0 ? 0.0f : 320.0f;
        value.freq_low = motor == 0 ? 160.0f : static_cast<float>(motor) / 204.0f;
        value.freq_high = triggerMotor == 0 ? 320.0f : static_cast<float>(triggerMotor) / 204.0f;
        hidSendVibrationValues(&m_vibration_device_handles[padIndex][handleIndex], &value, 1);
        return;
    }

    sendRumbleInternal(m_vibration_device_handles[padIndex], m_vibration_values[padIndex], lowFreqMotor, highFreqMotor, leftTriggerFreqMotor, rightTriggerFreqMotor);
}

void SwitchInputManager::updateMouseStates(RawMouseState* state)
{
    *state          = { };
    state->position = lastCursorPosition;
    if (currentMouseState.attributes & HidMouseAttribute_IsConnected)
    {
#ifdef __SWITCH2__
        state->position = Point(
            switchTouchCoordinate(currentMouseState.x, SwitchMouse::WIDTH, Application::windowWidth, Application::windowScale),
            switchTouchCoordinate(currentMouseState.y, SwitchMouse::HEIGHT, Application::windowHeight, Application::windowScale));
#else
        state->position = Point(currentMouseState.x, currentMouseState.y);
#endif
        state->offset = Point(currentMouseState.delta_x, currentMouseState.delta_y);
#ifdef __SWITCH2__
        state->scroll = Point(currentMouseState.menu_scroll_x * menuScrollSensitivity,
            currentMouseState.menu_scroll_y * menuScrollSensitivity);
#else
        state->scroll = Point(currentMouseState.wheel_delta_x, currentMouseState.wheel_delta_y);
#endif
        state->leftButton   = currentMouseState.buttons & HidMouseButton_Left;
        state->middleButton = currentMouseState.buttons & HidMouseButton_Middle;
        state->rightButton  = currentMouseState.buttons & HidMouseButton_Right;
        lastCursorPosition  = state->position;
    }
}

void SwitchInputManager::runloopStart()
{
    refreshHalfJoyConClaims();
    upToDateMouseState();
    handleMouse();
    handleKeyboard();
    handleControllerSensors();
}

void SwitchInputManager::upToDateMouseState()
{
    this->currentMouseState = this->mouse.update();
#ifdef __SWITCH2__
    const auto& pointer = this->mouse.joyCon();
    if (!this->joyConMotionLogged && (pointer.delta_x || pointer.delta_y))
    {
        Logger::info("SWITCH2_JOYCON_MOUSE: first motion side={} dx={} dy={}", static_cast<int>(pointer.side), pointer.delta_x, pointer.delta_y);
        this->joyConMotionLogged = true;
    }
    if (pointer.buttons_cur != pointer.buttons_old)
        Logger::info("SWITCH2_JOYCON_MOUSE: buttons=0x{:x} previous=0x{:x}", pointer.buttons_cur, pointer.buttons_old);
    if (!this->joyConScrollLogged && pointer.wheel_delta_y)
    {
        Logger::info("SWITCH2_JOYCON_MOUSE: first scroll raw={} wheelClicks={:.4f}",
            pointer.wheel_delta_y, pointer.wheel_delta_y / SwitchMouse::JOYCON_SCROLL_UNITS_PER_CLICK);
        this->joyConScrollLogged = true;
    }
#endif
}

void SwitchInputManager::handleMouse()
{
    if (currentMouseState.attributes & HidMouseAttribute_IsConnected)
    {
        getMouseCusorOffsetChanged()->fire(Point(currentMouseState.delta_x, currentMouseState.delta_y));
        getMouseScrollOffsetChanged()->fire(Point(currentMouseState.wheel_delta_x, currentMouseState.wheel_delta_y));
    }
}

void SwitchInputManager::handleKeyboard()
{
    HidKeyboardState state;

    if (hidGetKeyboardStates(&state, 1))
    {
        for (int i = 0; i < 256; ++i)
        {
            auto is_pressed = (state.keys[i / 64] & (1ul << (i % 64))) != 0;
            if (m_hid_keyboard_state[i] != is_pressed)
            {
                m_hid_keyboard_state[i]            = is_pressed;
                const BrlsKeyboardScancode brlsKey = m_hid_brls_map[i];

                KeyState keyState{};
                keyState.key     = brlsKey;
                keyState.pressed = is_pressed;

                if (state.modifiers & HidKeyboardModifier_LeftAlt)
                    keyState.mods |= BRLS_KBD_MODIFIER_ALT;

                if (state.modifiers & HidKeyboardModifier_Control)
                    keyState.mods |= BRLS_KBD_MODIFIER_CTRL;

                if (state.modifiers & HidKeyboardModifier_Shift)
                    keyState.mods |= BRLS_KBD_MODIFIER_SHIFT;

                if (state.modifiers & HidKeyboardModifier_Gui)
                    keyState.mods |= BRLS_KBD_MODIFIER_META;

                getKeyboardKeyStateChanged()->fire(keyState);
            }
        }
    }
    else
    {
        Logger::debug("Keyboard failed!");
    }
}

// Optional diagnostics, off by default. Create an empty file
// sdmc:/switch/Moonlight-Switch/sixaxis_debug.enable and restart the app to get
// sdmc:/switch/Moonlight-Switch/sixaxis_debug.log (per-controller sensor data, ~1 line/s per pad).
static void sixAxisDebug(const char* fmt, ...)
{
    static int state = -1;
    static FILE* file = nullptr;
    static int lines = 0;
    if (state < 0)
    {
        if (FILE* enable = fopen("sdmc:/switch/Moonlight-Switch/sixaxis_debug.enable", "r"))
        {
            fclose(enable);
            file = fopen("sdmc:/switch/Moonlight-Switch/sixaxis_debug.log", "w");
        }
        state = file ? 1 : 0;
    }
    if (!file || lines >= 3000)
        return;
    lines++;
    va_list args;
    va_start(args, fmt);
    vfprintf(file, fmt, args);
    va_end(args);
    fputc('\n', file);
    fflush(file);
}

// Per-side axis remap applied on top of the default conversion (after conversion to the Moonlight/DS4 frame).
// out[k] = in[axis[k]] * sign[k]. A single Joy-Con held horizontally is rotated relative to the sensor frame the
// default conversion assumes, and the left/right ones are mirrored, so these two tables are the place to calibrate
// on device (watch the motion in a gamepad tester on the host). Identity = same behaviour as a full controller.
struct SixAxisRemap
{
    int axis[3];
    float sign[3];
};
static const SixAxisRemap SIXAXIS_REMAP_DEFAULT       = { { 0, 1, 2 }, { 1.0f, 1.0f, 1.0f } };
static const SixAxisRemap SIXAXIS_REMAP_JOYCON_LEFT   = { { 0, 1, 2 }, { 1.0f, 1.0f, 1.0f } };
static const SixAxisRemap SIXAXIS_REMAP_JOYCON_RIGHT  = { { 0, 1, 2 }, { 1.0f, 1.0f, 1.0f } };

static const SixAxisRemap& sixAxisRemapFor(HalfJoyConSide side)
{
    switch (side)
    {
        case HalfJoyConSide::LEFT: return SIXAXIS_REMAP_JOYCON_LEFT;
        case HalfJoyConSide::RIGHT: return SIXAXIS_REMAP_JOYCON_RIGHT;
        default: return SIXAXIS_REMAP_DEFAULT;
    }
}

void SwitchInputManager::refreshPadSixAxis(int pad)
{
    PadSixAxis& sixAxis = m_pad_six_axis[pad];
    const u32 style     = padsState[pad].style_set;
    if (sixAxis.style == style)
        return;

    for (int i = 0; i < sixAxis.count; i++)
        hidStopSixAxisSensor(sixAxis.handles[i]);
    sixAxis.count = 0;
    sixAxis.style = style;

    HidNpadStyleTag tag = HidNpadStyleTag_NpadFullKey;
    int handleCount     = 1;
    if (style & HidNpadStyleTag_NpadFullKey)
        tag = HidNpadStyleTag_NpadFullKey;
    else if (style & HidNpadStyleTag_NpadJoyDual)
    {
        tag         = HidNpadStyleTag_NpadJoyDual;
        handleCount = 2; // [0] = left Joy-Con, [1] = right Joy-Con
    }
    else if (style & HidNpadStyleTag_NpadJoyLeft)
        tag = HidNpadStyleTag_NpadJoyLeft;
    else if (style & HidNpadStyleTag_NpadJoyRight)
        tag = HidNpadStyleTag_NpadJoyRight;
    else
    {
        // style == 0: pad disconnected, handles were released above and are re-acquired on reconnection
        if (style != 0)
            Logger::info("SIXAXIS: pad={} style=0x{:x} has no sixaxis", pad, style);
        return;
    }

    const Result handleResult = hidGetSixAxisSensorHandles(
        sixAxis.handles, handleCount, (HidNpadIdType)pad, tag);
    if (R_FAILED(handleResult))
    {
        Logger::warning("SIXAXIS: pad={} style=0x{:x} handle acquisition failed rc=0x{:x}",
            pad, style, handleResult);
        sixAxisDebug("ACQUIRE_FAILED pad=%d style=0x%x tag=0x%x rc=0x%x",
            pad, style, static_cast<unsigned>(tag), handleResult);
        return;
    }

    for (int i = 0; i < handleCount; i++)
    {
        const Result startResult = hidStartSixAxisSensor(sixAxis.handles[i]);
        if (R_FAILED(startResult))
        {
            for (int started = 0; started < i; started++)
                hidStopSixAxisSensor(sixAxis.handles[started]);
            Logger::warning("SIXAXIS: pad={} style=0x{:x} start failed handle={} rc=0x{:x}",
                pad, style, i, startResult);
            sixAxisDebug("START_FAILED pad=%d style=0x%x handle=%d rc=0x%x",
                pad, style, i, startResult);
            return;
        }
    }

    sixAxis.count = handleCount;
    Logger::info("SIXAXIS: pad={} style=0x{:x} tag=0x{:x} handles={} acquired",
        pad, style, static_cast<unsigned>(tag), handleCount);
    sixAxisDebug("ACQUIRE pad=%d style=0x%x tag=0x%x handles=%d",
        pad, style, static_cast<unsigned>(tag), handleCount);
}

void SwitchInputManager::handleControllerSensors()
{
    padUpdate(&this->padStateHandheld);
    const bool handheld = padStateHandheld.active_handheld;
    int controllerIndex = 0;
    static unsigned debugFrame = 0;
    const bool debugTick       = (debugFrame++ % 60) == 0;

    auto fire = [&](int index, const HidSixAxisSensorState& sixaxis, HalfJoyConSide side)
    {
        const float accel[3] = { -sixaxis.acceleration.x, -sixaxis.acceleration.z, sixaxis.acceleration.y };
        const float gyro[3]  = { static_cast<float>(sixaxis.angular_velocity.x * 2.0f * M_PI),
             static_cast<float>(sixaxis.angular_velocity.z * 2.0f * M_PI),
             static_cast<float>(-sixaxis.angular_velocity.y * 2.0f * M_PI) };
        const SixAxisRemap& remap = sixAxisRemapFor(side);

        auto accelState = SensorEvent{ index, SensorEventType::ACCEL, { 0, 0, 0 }, 0 };
        auto gyroState  = SensorEvent{ index, SensorEventType::GYRO, { 0, 0, 0 }, 0 };
        for (int k = 0; k < 3; k++)
        {
            accelState.data[k] = accel[remap.axis[k]] * remap.sign[k];
            gyroState.data[k]  = gyro[remap.axis[k]] * remap.sign[k];
        }
        getControllerSensorStateChanged()->fire(accelState);
        getControllerSensorStateChanged()->fire(gyroState);
    };

    if (handheld)
    {
        HidSixAxisSensorState sixaxis = { 0 };
        const size_t samples = m_six_axis_sensor_handheld_active
            ? hidGetSixAxisSensorStates(this->m_six_axis_sensor_handle_handheld, &sixaxis, 1)
            : 0;
        if (samples > 0)
            fire(controllerIndex, sixaxis, HalfJoyConSide::NONE);
        else if (debugTick)
            sixAxisDebug("DATA ctl=%d handheld=1 samples=0", controllerIndex);
        controllerIndex++;
    }

    // Same logical ordering as getControllersConnectedCount() / updateControllerState().
    for (int i = 0; i < GAMEPADS_MAX && controllerIndex < GAMEPADS_MAX; i++)
    {
        padUpdate(&this->padsState[i]);
        refreshPadSixAxis(i);
        if (padsState[i].style_set == 0)
            break;

        const u64 attrib = padGetAttributes(&padsState[i]);
        const PadSixAxis& padSixAxis = m_pad_six_axis[i];
        const bool joyDual = (padsState[i].style_set & HidNpadStyleTag_NpadJoyDual) != 0;
        int logicalOffset = 0;

        for (int sensor = 0; sensor < padSixAxis.count; sensor++)
        {
            const bool connected = !joyDual
                || (sensor == 0 && (attrib & HidNpadAttribute_IsLeftConnected))
                || (sensor == 1 && (attrib & HidNpadAttribute_IsRightConnected));
            if (!connected)
                continue;

            const int eventController = controllerIndex + logicalOffset;
            HidSixAxisSensorState sixaxis = { 0 };
            const size_t samples = hidGetSixAxisSensorStates(padSixAxis.handles[sensor], &sixaxis, 1);
            const HalfJoyConSide side = joyDual
                ? (sensor == 0 ? HalfJoyConSide::LEFT : HalfJoyConSide::RIGHT)
                : halfJoyConSide(padsState[i].style_set);

            if (debugTick)
                sixAxisDebug("DATA ctl=%d pad=%d style=0x%x attrib=0x%llx handles=%d selected=%d samples=%zu "
                             "gyro=(%.3f %.3f %.3f) acc=(%.3f %.3f %.3f)",
                    eventController, i, padsState[i].style_set, (unsigned long long)attrib, padSixAxis.count,
                    sensor, samples, sixaxis.angular_velocity.x, sixaxis.angular_velocity.y,
                    sixaxis.angular_velocity.z, sixaxis.acceleration.x, sixaxis.acceleration.y,
                    sixaxis.acceleration.z);

            if (samples > 0)
                fire(eventController, sixaxis, side);
            logicalOffset++;
        }
        controllerIndex += joyDual ? logicalOffset : 1;
    }
    Application::setActiveEvent(true);
}

void SwitchInputManager::setPointerLock(bool lock)
{
    pointerLocked = lock;
}

void SwitchInputManager::drawCursor(NVGcontext* vg)
{
    initCursor(vg);
    if (!pointerLocked)
    {
        this->paint.xform[4] = lastCursorPosition.x;
        this->paint.xform[5] = lastCursorPosition.y;

        nvgBeginPath(vg);
        nvgRect(vg, lastCursorPosition.x, lastCursorPosition.y, this->cursorWidth, this->cursorHeight);
        nvgFillPaint(vg, this->paint);
        nvgFill(vg);
    }
}

void SwitchInputManager::initCursor(NVGcontext* vg)
{
    if (cursorInited)
        return;
    if (vg)
    {
#ifdef USE_LIBROMFS
        auto image          = romfs::get("img/sys/cursor.png");
        this->cursorTexture = nvgCreateImageMem(vg, NVG_IMAGE_NEAREST, (unsigned char*)image.data(), image.size());
#else
        this->cursorTexture = nvgCreateImage(vg, BRLS_ASSET("img/sys/cursor.png"), NVG_IMAGE_NEAREST);
#endif

        int width, height;
        nvgImageSize(vg, cursorTexture, &width, &height);
        float aspect       = (float)height / (float)width;
        this->cursorWidth  = 18;
        this->cursorHeight = 18 * aspect;

        this->paint        = nvgImagePattern(vg, 0, 0, this->cursorWidth, this->cursorHeight, 0, this->cursorTexture, 1.0f);
        this->cursorInited = true;
    }
}

static BrlsKeyboardScancode hidKey2BrlsKey(int key)
{
    if (KBD_A <= key && key <= KBD_Z)
    {
        return (BrlsKeyboardScancode)(key - KBD_A + BRLS_KBD_KEY_A);
    }
    if (KBD_1 <= key && key <= KBD_9)
    {
        return (BrlsKeyboardScancode)(key - KBD_1 + BRLS_KBD_KEY_1);
    }
    if (KBD_F1 <= key && key <= KBD_F12)
    {
        return (BrlsKeyboardScancode)(key - KBD_F1 + BRLS_KBD_KEY_F1);
    }
    if (KBD_KP1 <= key && key <= KBD_KP9)
    {
        return (BrlsKeyboardScancode)(key - KBD_KP1 + BRLS_KBD_KEY_KP_1);
    }

    switch (key)
    {
        case KBD_0:
            return BRLS_KBD_KEY_0;
        case KBD_SPACE:
            return BRLS_KBD_KEY_SPACE;
        case KBD_APOSTROPHE:
            return BRLS_KBD_KEY_APOSTROPHE;
        case KBD_COMMA:
            return BRLS_KBD_KEY_COMMA;
        case KBD_MINUS:
            return BRLS_KBD_KEY_MINUS;
        case KBD_DOT:
            return BRLS_KBD_KEY_PERIOD;
        case KBD_SLASH:
            return BRLS_KBD_KEY_SLASH;
        case KBD_SEMICOLON:
            return BRLS_KBD_KEY_SEMICOLON;
        case KBD_EQUAL:
            return BRLS_KBD_KEY_EQUAL;
        case KBD_LEFTBRACE:
            return BRLS_KBD_KEY_LEFT_BRACKET;
        case KBD_RIGHTBRACE:
            return BRLS_KBD_KEY_RIGHT_BRACKET;
        case KBD_BACKSLASH:
            return BRLS_KBD_KEY_BACKSLASH;
        case KBD_GRAVE:
            return BRLS_KBD_KEY_GRAVE_ACCENT;
        case KBD_ESC:
            return BRLS_KBD_KEY_ESCAPE;
        case KBD_ENTER:
            return BRLS_KBD_KEY_ENTER;
        case KBD_TAB:
            return BRLS_KBD_KEY_TAB;
        case KBD_BACKSPACE:
            return BRLS_KBD_KEY_BACKSPACE;
        case KBD_CAPSLOCK:
            return BRLS_KBD_KEY_CAPS_LOCK;
        case KBD_LEFTSHIFT:
            return BRLS_KBD_KEY_LEFT_SHIFT;
        case KBD_LEFTCTRL:
            return BRLS_KBD_KEY_LEFT_CONTROL;
        case KBD_LEFTALT:
            return BRLS_KBD_KEY_LEFT_ALT;
        case KBD_LEFTMETA:
            return BRLS_KBD_KEY_LEFT_SUPER;
        case KBD_RIGHTSHIFT:
            return BRLS_KBD_KEY_RIGHT_SHIFT;
        case KBD_RIGHTCTRL:
            return BRLS_KBD_KEY_RIGHT_CONTROL;
        case KBD_RIGHTALT:
            return BRLS_KBD_KEY_RIGHT_ALT;
        case KBD_RIGHTMETA:
            return BRLS_KBD_KEY_RIGHT_SUPER;
        case KBD_LEFT:
            return BRLS_KBD_KEY_LEFT;
        case KBD_RIGHT:
            return BRLS_KBD_KEY_RIGHT;
        case KBD_UP:
            return BRLS_KBD_KEY_UP;
        case KBD_DOWN:
            return BRLS_KBD_KEY_DOWN;

        case KBD_SYSRQ:
            return BRLS_KBD_KEY_PRINT_SCREEN;
        case KBD_SCROLLLOCK:
            return BRLS_KBD_KEY_SCROLL_LOCK;
        case KBD_PAUSE:
            return BRLS_KBD_KEY_PAUSE;
        case KBD_INSERT:
            return BRLS_KBD_KEY_INSERT;
        case KBD_HOME:
            return BRLS_KBD_KEY_HOME;
        case KBD_PAGEUP:
            return BRLS_KBD_KEY_PAGE_UP;
        case KBD_DELETE:
            return BRLS_KBD_KEY_DELETE;
        case KBD_END:
            return BRLS_KBD_KEY_END;
        case KBD_PAGEDOWN:
            return BRLS_KBD_KEY_PAGE_DOWN;

        case KBD_NUMLOCK:
            return BRLS_KBD_KEY_NUM_LOCK;
        case KBD_KPSLASH:
            return BRLS_KBD_KEY_KP_DIVIDE;
        case KBD_KPASTERISK:
            return BRLS_KBD_KEY_KP_MULTIPLY;
        case KBD_KPMINUS:
            return BRLS_KBD_KEY_KP_SUBTRACT;
        case KBD_KPPLUS:
            return BRLS_KBD_KEY_KP_ADD;
        case KBD_KPENTER:
            return BRLS_KBD_KEY_KP_ENTER;
        case KBD_KPDOT:
            return BRLS_KBD_KEY_KP_DECIMAL;
        case KBD_KP0:
            return BRLS_KBD_KEY_KP_0;

        case KBD_102ND:
            return BRLS_KBD_KEY_WORLD_1;

        // case KBD_HASHTILDE: return GLFW_HASHTILDE;
        default:
            return BRLS_KBD_KEY_UNKNOWN;
    }
}

void SwitchInputManager::hidInitializeKeyboardMap()
{
    m_hid_brls_map[KBD_NONE]             = BRLS_KBD_KEY_UNKNOWN;
    m_brls_hid_map[BRLS_KBD_KEY_UNKNOWN] = KBD_NONE;

    for (int i = 0; i < 256; ++i)
    {
        const auto brlsKey = hidKey2BrlsKey(i);
        if (brlsKey == BRLS_KBD_KEY_UNKNOWN)
            continue;
        m_hid_brls_map[i]       = brlsKey;
        m_brls_hid_map[brlsKey] = i;
    }
}

void SwitchInputManager::setHalfJoyConMapping(HalfJoyConSide side, const HalfJoyConMapping& mapping)
{
    if (side == HalfJoyConSide::NONE)
        return;
    auto& target = halfMappings[side == HalfJoyConSide::LEFT ? 0 : 1];
    for (size_t i = 0; i < mapping.size(); ++i)
        target[i] = validHalfJoyConAction(mapping[i]) ? mapping[i] : HALF_JOYCON_UNMAPPED;
    refreshHalfJoyConClaims();
}

void SwitchInputManager::setHalfJoyConMappingEnabled(bool enabled)
{
    halfMappingEnabled = enabled;
    refreshHalfJoyConClaims();
    if (!enabled)
    {
        if (halfSystemConsumed[0].exchange(false))
            m_isScreenshotPressed = false;
        if (halfSystemConsumed[1].exchange(false))
            m_isHomePressed = false;
#ifdef __SWITCH2__
        if (halfSystemConsumed[2].exchange(false))
            m_isChatPressed = false;
#endif
    }
}

bool SwitchInputManager::isHalfJoyConMappingActive(int controller) const
{
    if (!halfMappingEnabled || controller < 0 || controller >= GAMEPADS_MAX)
        return false;
    if (padStateHandheld.active_handheld)
    {
        if (controller == 0)
            return false;
        --controller;
    }
    int logicalIndex = 0;
    for (int i = 0; i < GAMEPADS_MAX; ++i)
    {
        const_cast<SwitchInputManager*>(this)->padUpdate(&padsState[i]);
        if (padsState[i].style_set == 0)
            break;
        if (padsState[i].style_set & HidNpadStyleTag_NpadJoyDual)
        {
            const u64 attrib = padGetAttributes(&padsState[i]);
            if (attrib & HidNpadAttribute_IsLeftConnected)
            {
                if (logicalIndex++ == controller)
                    return true;
            }
            if (attrib & HidNpadAttribute_IsRightConnected)
            {
                if (logicalIndex++ == controller)
                    return true;
            }
        }
        else
        {
            if (logicalIndex++ == controller)
                return halfJoyConSide(padsState[i].style_set) != HalfJoyConSide::NONE;
        }
    }
    return false;
}

void SwitchInputManager::refreshHalfJoyConClaims()
{
    u32 combined                  = 0;
    std::array<u64, 3> candidates = { };
    if (halfMappingEnabled)
    {
        padUpdate(&padStateHandheld);
        if (padStateHandheld.active_handheld)
            for (auto& candidate : candidates)
                candidate |= 1ULL << HidNpadIdType_Handheld;
    }
    for (int i = 0; i < GAMEPADS_MAX; ++i)
    {
        u32 claims = 0;
        if (halfMappingEnabled)
        {
            padUpdate(&padsState[i]);
            const auto side = halfJoyConSide(padsState[i].style_set);
            if (padsState[i].style_set & HidNpadStyleTag_NpadJoyDual)
            {
                const u64 attrib = padGetAttributes(&padsState[i]);
                if (attrib & HidNpadAttribute_IsLeftConnected)
                {
                    candidates[0] |= 1ULL << i;
                    claims |= halfJoyConSystemClaims(halfMappings[0], HalfJoyConSide::LEFT);
                }
                if (attrib & HidNpadAttribute_IsRightConnected)
                {
                    candidates[1] |= 1ULL << i;
                    candidates[2] |= 1ULL << i;
                    claims |= halfJoyConSystemClaims(halfMappings[1], HalfJoyConSide::RIGHT);
                }
            }
            else if (padsState[i].style_set)
            {
                const u32 supported = side == HalfJoyConSide::LEFT ? 1U : side == HalfJoyConSide::RIGHT ? 6U
                                                                                                        : 7U;
                for (size_t button = 0; button < candidates.size(); ++button)
                    if (supported & (1U << button))
                        candidates[button] |= 1ULL << i;
                if (side != HalfJoyConSide::NONE)
                    claims = halfJoyConSystemClaims(halfMappings[side == HalfJoyConSide::LEFT ? 0 : 1], side);
            }
        }
        if (halfSystemClaims[i].exchange(claims) != claims)
            Logger::info("HALF_JOYCON: pad={} claims=0x{:x}", i, claims);
        combined |= claims;
    }
    halfAnyClaims = combined;
    for (size_t button = 0; button < candidates.size(); ++button)
        halfSourceCandidates[button] = candidates[button];
}

void SwitchInputManager::applyHalfMapping(ControllerState* state, PadState* pad, u64 buttons)
{
    const auto side = halfJoyConSide(pad->style_set);
    if (!halfMappingEnabled || side == HalfJoyConSide::NONE)
        return;
    int owner = -1;
    for (int i = 0; i < GAMEPADS_MAX; ++i)
        if (pad->id_mask & (1U << i))
        {
            owner = i;
            break;
        }
    const int system         = side == HalfJoyConSide::LEFT ? 0 : 1;
    const bool held          = system == 0 ? m_isScreenshotPressed.load() : m_isHomePressed.load();
    const bool systemPressed = held && halfSystemConsumed[system] && systemButtonOwners[system] == owner;
    bool chatPressed         = false;
#ifdef __SWITCH2__
    chatPressed = side == HalfJoyConSide::RIGHT && m_isChatPressed && halfSystemConsumed[2] && systemButtonOwners[2] == owner;
#endif
    applyHalfJoyConMapping(state, halfMappings[side == HalfJoyConSide::LEFT ? 0 : 1], side, buttons, systemPressed, chatPressed);
}

void SwitchInputManager::processSystemButtonEvent(::Event* event, int button)
{
    std::atomic<ButtonOverrideMode>* mode = button == 0 ? &m_screenshotButtonMode : &m_homeButtonMode;
    std::atomic<bool>* held               = button == 0 ? &m_isScreenshotPressed : &m_isHomePressed;
#ifdef __SWITCH2__
    if (button == 2)
    {
        mode = &m_chatButtonMode;
        held = &m_isChatPressed;
    }
#endif
    const bool wasPressed = held->load();
    bool halfConsumed     = halfSystemConsumed[button];
    int owner             = systemButtonOwners[button];
    if (!wasPressed)
    {
        if (mode->load() == ButtonOverrideMode::NONE && !(halfAnyClaims.load() & (1U << button)))
            return;
        u32 active               = 0;
        const Result result      = hidsysGetLastActiveNpad(&active);
        owner                    = R_SUCCEEDED(result) ? static_cast<int>(active) : -1;
        const u64 candidates     = halfSourceCandidates[button];
        const bool possibleOwner = owner >= 0 && owner < 64 && (candidates & (1ULL << owner));
        if (halfMappingEnabled && (R_FAILED(result) || !possibleOwner))
        {
            // A right half has no Capture button, for example. Infer ownership
            // only if exactly one connected controller can produce this event.
            owner = -1;
            if (candidates && !(candidates & (candidates - 1)))
                for (int i = 0; i < 64; ++i)
                    if (candidates & (1ULL << i))
                    {
                        owner = i;
                        break;
                    }
        }
        halfConsumed = halfMappingEnabled && owner >= 0 && owner < GAMEPADS_MAX
            && (halfSystemClaims[owner].load() & (1U << button));
    }
    if (!halfConsumed && mode->load() == ButtonOverrideMode::NONE)
        return;
    eventClear(event);
    systemButtonOwners[button] = owner;
    halfSystemConsumed[button] = !wasPressed && halfConsumed;
    *held                      = !wasPressed;
    Logger::info("SWITCH_SYSTEM_BUTTON: button={} owner={} pressed={} half={} mode={}",
        button, owner, !wasPressed, halfConsumed, static_cast<int>(mode->load()));
}

void SwitchInputManager::system_button_thread_fn(std::stop_token token)
{
    svcSetThreadPriority(CUR_THREAD_HANDLE, 0x20);
    NXEvent screenshot_evt = { };
    if (auto rc = hidsysAcquireCaptureButtonEventHandle(&screenshot_evt, false); R_FAILED(rc))
    {
        Logger::error("Failed to acquire the screenshot button event: {}", rc);
        return;
    }
    DEFER([&screenshot_evt]
        { eventClose(&screenshot_evt); });
    NXEvent home_evt = { };
    if (auto rc = hidsysAcquireHomeButtonEventHandle(&home_evt, false); R_FAILED(rc))
    {
        Logger::error("Failed to acquire the home button event: {}", rc);
        return;
    }
    DEFER([&home_evt]
        { eventClose(&home_evt); });
#ifdef __SWITCH2__
    NXEvent chat_evt        = { };
    const Result chatResult = acquireSwitchChatButtonEvent(&chat_evt, false);
    const bool hasChatEvent = R_SUCCEEDED(chatResult);
    DEFER([&]
        { if (hasChatEvent) eventClose(&chat_evt); });
    Logger::info("SWITCH2_CHAT_BUTTON: acquireEvent rc=0x{:x} available={}", chatResult, hasChatEvent);
    if (hasChatEvent)
        eventClear(&chat_evt);
#endif
    eventClear(&screenshot_evt);
    eventClear(&home_evt);
    while (!token.stop_requested())
    {
        s32 idx               = -1;
        const Waiter events[] = {
            waiterForEvent(&screenshot_evt),
            waiterForEvent(&home_evt),
#ifdef __SWITCH2__
            waiterForEvent(&chat_evt),
#endif
        };
#ifdef __SWITCH2__
        const int eventCount = hasChatEvent ? 3 : 2;
#else
        const int eventCount = 2;
#endif
        const Result rc = waitObjects(&idx, events, eventCount, std::chrono::nanoseconds(50ms).count());
        if (rc == KERNELRESULT(TimedOut))
            continue;
        if (R_FAILED(rc))
        {
            Logger::warning("System button event wait failed: 0x{:x}", rc);
            break;
        }
        if (idx == 0)
            processSystemButtonEvent(&screenshot_evt, 0);
        else if (idx == 1)
            processSystemButtonEvent(&home_evt, 1);
#ifdef __SWITCH2__
        else if (idx == 2)
            processSystemButtonEvent(&chat_evt, 2);
#endif
    }
    m_isScreenshotPressed = false;
    m_isHomePressed       = false;
#ifdef __SWITCH2__
    m_isChatPressed = false;
#endif
}

} // namespace brls
