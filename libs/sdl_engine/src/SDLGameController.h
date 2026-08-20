#pragma once

#include <SDL.h>
#include <array>
#include <unordered_map>

class GameController {
public:
    struct ButtonState {
        bool down = false;
        bool pressed = false;
    };

    void OnButtonStateChange(int buttonIndex, bool down) {
        auto& state = m_buttonStates[buttonIndex];
        if (down) {
            state.pressed = !state.down;
            state.down = true;
        } else {
            state.down = false;
        }
    }

    void OnAxisStateChange(int axisIndex, int32_t value) { m_axisValue[axisIndex] = value; }

    void PostFrameUpdateStates() {
        for (auto& state : m_buttonStates) {
            state.pressed = false;
        }
    }

    const ButtonState& GetButtonState(int buttonIndex) const { return m_buttonStates[buttonIndex]; }

    const int32_t& GetAxisValue(int axisValue) const { return m_axisValue[axisValue]; }

private:
    std::array<ButtonState, SDL_CONTROLLER_BUTTON_MAX> m_buttonStates;
    std::array<int32_t, SDL_CONTROLLER_AXIS_MAX> m_axisValue = {0};
};

class SDLGameControllerDriver {
public:
    // Most gamepads we track. AddController refuses anything beyond this, so an index is only
    // ever in [0, MaxControllers).
    static constexpr int MaxControllers = 2;

    // Call once per frame
    void PostFrameUpdateKeyStates();

    void AddController(int index);
    void RemoveController(int instanceId);
    int NumControllers() const;

    bool IsControllerConnected(int index) const;

    // Null when no such controller is connected.
    //
    // These deliberately return pointers rather than references: which controllers exist is
    // driven by hotplug events and by a persisted user setting, so "no controller at this
    // index" is a normal state that every caller has to handle, not a programming error. The
    // earlier reference-returning versions asserted and then dereferenced a past-the-end
    // iterator, which crashed the process in a release build.
    GameController* ControllerByInstanceId(int instanceId);
    const GameController* ControllerByIndex(int index) const;

private:
    std::unordered_map<int, GameController> m_playerIndexToGamepad;
    std::unordered_map<int, int> m_instanceIdToPlayerIndex;
};
