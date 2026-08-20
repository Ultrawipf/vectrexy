#include "SDLGameController.h"
#include <cassert>
#include <core/ConsoleOutput.h>

void SDLGameControllerDriver::PostFrameUpdateKeyStates() {
    for (auto& kvp : m_playerIndexToGamepad) {
        kvp.second.PostFrameUpdateStates();
    }
}

void SDLGameControllerDriver::AddController(int index) {
    if (index >= MaxControllers) {
        Printf("Cannot support more than %d gamepads\n", MaxControllers);
        return;
    }

    if (SDL_IsGameController(index)) {
        SDL_GameController* controller = SDL_GameControllerOpen(index);
        if (controller) {
            auto joy = SDL_GameControllerGetJoystick(controller);
            auto instanceId = SDL_JoystickInstanceID(joy);
            m_instanceIdToPlayerIndex[instanceId] = index;
            m_playerIndexToGamepad[index] = {};
        }
    }
}

void SDLGameControllerDriver::RemoveController(int instanceId) {
    if (auto* controller = SDL_GameControllerFromInstanceID(instanceId)) {
        SDL_GameControllerClose(controller);
    }

    // SDL reports removal for every gamepad, including ones AddController refused because they
    // were beyond MaxControllers, so an unknown instance id is expected rather than an error.
    // Looking the index up with operator[] used to insert a bogus 0 entry for exactly those.
    auto instanceIter = m_instanceIdToPlayerIndex.find(instanceId);
    if (instanceIter == m_instanceIdToPlayerIndex.end())
        return;

    m_playerIndexToGamepad.erase(instanceIter->second);
    m_instanceIdToPlayerIndex.erase(instanceIter);
}

int SDLGameControllerDriver::NumControllers() const {
    return static_cast<int>(m_playerIndexToGamepad.size());
}

bool SDLGameControllerDriver::IsControllerConnected(int index) const {
    return m_playerIndexToGamepad.find(index) != m_playerIndexToGamepad.end();
}

GameController* SDLGameControllerDriver::ControllerByInstanceId(int instanceId) {
    auto instanceIter = m_instanceIdToPlayerIndex.find(instanceId);
    if (instanceIter == m_instanceIdToPlayerIndex.end())
        return nullptr;
    auto iter = m_playerIndexToGamepad.find(instanceIter->second);
    return iter != m_playerIndexToGamepad.end() ? &iter->second : nullptr;
}

const GameController* SDLGameControllerDriver::ControllerByIndex(int index) const {
    auto iter = m_playerIndexToGamepad.find(index);
    return iter != m_playerIndexToGamepad.end() ? &iter->second : nullptr;
}
