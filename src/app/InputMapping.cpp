#include "InputMapping.h"

#include <array>
#include <string>
#include <vector>

#include "util/Logger.h"

namespace {

constexpr Sint16 kAxisPressThreshold = 16000;
constexpr Sint16 kAxisReleaseThreshold = 12000;

SDL_GameController* g_controller = nullptr;
SDL_JoystickID g_controllerInstance = -1;
std::string g_controllerName;
int g_leftXAxisState = 0;
int g_leftYAxisState = 0;
std::array<int, 4> g_directionRefCounts{ 0, 0, 0, 0 };
bool g_gamepadBridgeInitialized = false;

int directionIndex(SDL_Keycode key) {
    switch (key) {
    case SDLK_UP:
        return 0;
    case SDLK_DOWN:
        return 1;
    case SDLK_LEFT:
        return 2;
    case SDLK_RIGHT:
        return 3;
    default:
        return -1;
    }
}

SDL_Keycode directionalKeyForButton(Uint8 button) {
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        return SDLK_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return SDLK_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return SDLK_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return SDLK_RIGHT;
    default:
        return SDLK_UNKNOWN;
    }
}

SDL_Keycode keyForControllerButton(Uint8 button) {
    if (const SDL_Keycode direction = directionalKeyForButton(button); direction != SDLK_UNKNOWN) {
        return direction;
    }
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A:
        return SDLK_RETURN;
    case SDL_CONTROLLER_BUTTON_B:
        return SDLK_ESCAPE;
    case SDL_CONTROLLER_BUTTON_START:
        return SDLK_p;
    default:
        return SDLK_UNKNOWN;
    }
}

SDL_Keycode keyForAxisState(SDL_GameControllerAxis axis, int state) {
    if (state == 0) {
        return SDLK_UNKNOWN;
    }
    if (axis == SDL_CONTROLLER_AXIS_LEFTX) {
        return state < 0 ? SDLK_LEFT : SDLK_RIGHT;
    }
    if (axis == SDL_CONTROLLER_AXIS_LEFTY) {
        return state < 0 ? SDLK_UP : SDLK_DOWN;
    }
    return SDLK_UNKNOWN;
}

int nextAxisState(Sint16 value, int currentState) {
    if (currentState < 0) {
        if (value <= -kAxisReleaseThreshold) {
            return -1;
        }
        if (value >= kAxisPressThreshold) {
            return 1;
        }
        return 0;
    }
    if (currentState > 0) {
        if (value >= kAxisReleaseThreshold) {
            return 1;
        }
        if (value <= -kAxisPressThreshold) {
            return -1;
        }
        return 0;
    }
    if (value <= -kAxisPressThreshold) {
        return -1;
    }
    if (value >= kAxisPressThreshold) {
        return 1;
    }
    return 0;
}

void rewriteAsKeyboardEvent(SDL_Event& event, Uint32 type, SDL_Keycode key) {
    const Uint32 timestamp = event.common.timestamp;
    event = SDL_Event{};
    event.type = type;
    event.key.type = type;
    event.key.timestamp = timestamp;
    event.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
    event.key.repeat = 0;
    event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    event.key.keysym.sym = key;
    event.key.keysym.mod = KMOD_NONE;
}

void pushKeyboardEvent(Uint32 type, SDL_Keycode key, Uint32 timestamp) {
    SDL_Event synthetic{};
    synthetic.common.timestamp = timestamp;
    rewriteAsKeyboardEvent(synthetic, type, key);
    if (SDL_PushEvent(&synthetic) < 0) {
        Logger::warn(std::string("Unable to queue synthetic gamepad key: ") + SDL_GetError(), __func__);
    }
}

bool applyDirectionalSourceChange(
    SDL_Event& event,
    SDL_Keycode releasedKey,
    SDL_Keycode pressedKey) {
    std::vector<std::pair<Uint32, SDL_Keycode>> emitted;

    if (releasedKey != SDLK_UNKNOWN) {
        const int index = directionIndex(releasedKey);
        if (index >= 0 && g_directionRefCounts[static_cast<std::size_t>(index)] > 0) {
            int& count = g_directionRefCounts[static_cast<std::size_t>(index)];
            --count;
            if (count == 0) {
                emitted.emplace_back(SDL_KEYUP, releasedKey);
            }
        }
    }

    if (pressedKey != SDLK_UNKNOWN) {
        const int index = directionIndex(pressedKey);
        if (index >= 0) {
            int& count = g_directionRefCounts[static_cast<std::size_t>(index)];
            ++count;
            if (count == 1) {
                emitted.emplace_back(SDL_KEYDOWN, pressedKey);
            }
        }
    }

    if (emitted.empty()) {
        return false;
    }

    const Uint32 timestamp = event.common.timestamp;
    rewriteAsKeyboardEvent(event, emitted.front().first, emitted.front().second);
    for (std::size_t index = 1; index < emitted.size(); ++index) {
        pushKeyboardEvent(emitted[index].first, emitted[index].second, timestamp);
    }
    return true;
}

void resetDirectionalState(Uint32 timestamp, bool emitReleases) {
    if (emitReleases) {
        constexpr std::array<SDL_Keycode, 4> keys{ SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT };
        for (std::size_t index = 0; index < keys.size(); ++index) {
            if (g_directionRefCounts[index] > 0) {
                pushKeyboardEvent(SDL_KEYUP, keys[index], timestamp);
            }
        }
    }
    g_directionRefCounts = { 0, 0, 0, 0 };
    g_leftXAxisState = 0;
    g_leftYAxisState = 0;
}

void closeController(Uint32 timestamp = 0, bool emitReleases = false) {
    if (!g_controller) {
        resetDirectionalState(timestamp, emitReleases);
        return;
    }

    const std::string disconnectedName = g_controllerName;
    resetDirectionalState(timestamp, emitReleases);
    SDL_GameControllerClose(g_controller);
    g_controller = nullptr;
    g_controllerInstance = -1;
    g_controllerName.clear();

    Logger::info(
        std::string("Gamepad disconnected") +
            (disconnectedName.empty() ? std::string{} : std::string(": ") + disconnectedName),
        __func__);
}

bool openController(int deviceIndex) {
    if (g_controller || deviceIndex < 0 || !SDL_IsGameController(deviceIndex)) {
        return false;
    }

    SDL_GameController* controller = SDL_GameControllerOpen(deviceIndex);
    if (!controller) {
        Logger::warn(std::string("Unable to open gamepad: ") + SDL_GetError(), __func__);
        return false;
    }

    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
    if (!joystick) {
        SDL_GameControllerClose(controller);
        Logger::warn("Unable to access gamepad joystick handle.", __func__);
        return false;
    }

    g_controller = controller;
    g_controllerInstance = SDL_JoystickInstanceID(joystick);
    const char* name = SDL_GameControllerName(controller);
    g_controllerName = name ? name : "";
    resetDirectionalState(0, false);

    Logger::info(
        std::string("Gamepad connected") +
            (g_controllerName.empty() ? std::string{} : std::string(": ") + g_controllerName),
        __func__);

    char* mapping = SDL_GameControllerMapping(controller);
    if (mapping) {
        Logger::debug(std::string("Gamepad mapping: ") + mapping, __func__);
        SDL_free(mapping);
    }

    return true;
}

void openFirstAvailableController() {
    if (g_controller) {
        return;
    }
    const int joystickCount = SDL_NumJoysticks();
    for (int index = 0; index < joystickCount; ++index) {
        if (openController(index)) {
            return;
        }
    }
}

} // namespace

void initializeGamepadInput() {
    if (g_gamepadBridgeInitialized) {
        return;
    }

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        Logger::warn(std::string("Gamepad support disabled: ") + SDL_GetError(), __func__);
        return;
    }

    g_gamepadBridgeInitialized = true;
    SDL_GameControllerEventState(SDL_ENABLE);
    openFirstAvailableController();
}

void shutdownGamepadInput() {
    if (!g_gamepadBridgeInitialized) {
        return;
    }
    closeController();
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    g_gamepadBridgeInitialized = false;
}

bool translateGamepadEvent(SDL_Event& event) {
    if (!g_gamepadBridgeInitialized) {
        return true;
    }

    if (event.type == SDL_CONTROLLERDEVICEADDED) {
        if (!g_controller) {
            openController(event.cdevice.which);
        }
        return false;
    }

    if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
        if (event.cdevice.which == g_controllerInstance) {
            // Do not immediately scan/reopen here. With sdl2-compat on top of
            // SDL3, the removed device can still appear in SDL_NumJoysticks()
            // while this event is being consumed, leading to a stale reopen.
            closeController(event.cdevice.timestamp, true);
        }
        return false;
    }

    if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
        if (event.cbutton.which != g_controllerInstance) {
            return false;
        }

        const bool pressed = event.type == SDL_CONTROLLERBUTTONDOWN;
        const SDL_Keycode key = keyForControllerButton(event.cbutton.button);
        Logger::debug(
            std::string("Gamepad button ") + (pressed ? "down" : "up") +
                ": " + std::to_string(static_cast<int>(event.cbutton.button)),
            __func__);

        if (key == SDLK_UNKNOWN) {
            return false;
        }

        if (directionalKeyForButton(event.cbutton.button) != SDLK_UNKNOWN) {
            return pressed ?
                applyDirectionalSourceChange(event, SDLK_UNKNOWN, key) :
                applyDirectionalSourceChange(event, key, SDLK_UNKNOWN);
        }

        rewriteAsKeyboardEvent(event, pressed ? SDL_KEYDOWN : SDL_KEYUP, key);
        return true;
    }

    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.which != g_controllerInstance ||
            (event.caxis.axis != SDL_CONTROLLER_AXIS_LEFTX &&
                event.caxis.axis != SDL_CONTROLLER_AXIS_LEFTY)) {
            return false;
        }

        int& axisState = event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ?
            g_leftXAxisState : g_leftYAxisState;
        const int previousState = axisState;
        const int requestedState = nextAxisState(event.caxis.value, previousState);
        if (requestedState == previousState) {
            return false;
        }

        axisState = requestedState;
        Logger::debug(
            std::string("Gamepad axis ") + std::to_string(static_cast<int>(event.caxis.axis)) +
                " state " + std::to_string(previousState) + " -> " + std::to_string(requestedState) +
                " value=" + std::to_string(event.caxis.value),
            __func__);

        const SDL_GameControllerAxis axis = static_cast<SDL_GameControllerAxis>(event.caxis.axis);
        const SDL_Keycode releasedKey = keyForAxisState(axis, previousState);
        const SDL_Keycode pressedKey = keyForAxisState(axis, requestedState);
        return applyDirectionalSourceChange(event, releasedKey, pressedKey);
    }

    return true;
}

std::optional<Direction> directionFromKey(SDL_Keycode key) {
    switch (key) {
    case SDLK_UP:
        return Direction::Up;
    case SDLK_DOWN:
        return Direction::Down;
    case SDLK_LEFT:
        return Direction::Left;
    case SDLK_RIGHT:
        return Direction::Right;
    default:
        return std::nullopt;
    }
}

SDL_Keycode parseKeyFromName(const std::string& name, SDL_Keycode fallback, const char* context) {
    if (name.empty()) {
        return fallback;
    }
    const SDL_Keycode parsed = SDL_GetKeyFromName(name.c_str());
    if (parsed == SDLK_UNKNOWN) {
        Logger::warn("Invalid key name '" + name + "', falling back to default.", context);
        return fallback;
    }
    return parsed;
}

bool isVolumeIncreaseKey(SDL_Keycode key) {
    return key == SDLK_PLUS || key == SDLK_KP_PLUS || key == SDLK_EQUALS;
}

bool isVolumeDecreaseKey(SDL_Keycode key) {
    return key == SDLK_MINUS || key == SDLK_KP_MINUS;
}
