#include "InputMapping.h"

#include <algorithm>
#include <string>

#include "util/Logger.h"

namespace {

constexpr Sint16 kAxisPressThreshold = 16000;
constexpr Sint16 kAxisReleaseThreshold = 12000;

SDL_GameController* g_controller = nullptr;
SDL_JoystickID g_controllerInstance = -1;
int g_leftXAxisState = 0;
int g_leftYAxisState = 0;
SDL_Keycode g_lastDirectionalKey = SDLK_UNKNOWN;
bool g_gamepadBridgeInitialized = false;

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

void closeController() {
    if (!g_controller) {
        return;
    }
    const char* name = SDL_GameControllerName(g_controller);
    Logger::info(
        std::string("Gamepad disconnected") + (name ? std::string(": ") + name : std::string{}),
        __func__);
    SDL_GameControllerClose(g_controller);
    g_controller = nullptr;
    g_controllerInstance = -1;
    g_leftXAxisState = 0;
    g_leftYAxisState = 0;
}

bool openController(int deviceIndex) {
    if (g_controller || deviceIndex < 0 || !SDL_IsGameController(deviceIndex)) {
        return false;
    }
    SDL_GameController* controller = SDL_GameControllerOpen(deviceIndex);
    if (!controller) {
        Logger::warn(
            std::string("Unable to open gamepad: ") + SDL_GetError(),
            __func__);
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
    Logger::info(
        std::string("Gamepad connected") + (name ? std::string(": ") + name : std::string{}),
        __func__);
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

int gamepadEventFilter(void*, SDL_Event* event) {
    if (!event) {
        return 1;
    }

    if (event->type == SDL_CONTROLLERDEVICEADDED) {
        openController(event->cdevice.which);
        return 0;
    }

    if (event->type == SDL_CONTROLLERDEVICEREMOVED) {
        if (event->cdevice.which != g_controllerInstance) {
            return 0;
        }
        const SDL_Keycode heldDirection = g_lastDirectionalKey;
        closeController();
        openFirstAvailableController();
        if (heldDirection != SDLK_UNKNOWN) {
            g_lastDirectionalKey = SDLK_UNKNOWN;
            rewriteAsKeyboardEvent(*event, SDL_KEYUP, heldDirection);
            return 1;
        }
        return 0;
    }

    if (event->type == SDL_CONTROLLERBUTTONDOWN || event->type == SDL_CONTROLLERBUTTONUP) {
        if (event->cbutton.which != g_controllerInstance) {
            return 0;
        }
        const SDL_Keycode key = keyForControllerButton(event->cbutton.button);
        if (key == SDLK_UNKNOWN) {
            return 0;
        }
        const bool pressed = event->type == SDL_CONTROLLERBUTTONDOWN;
        if (directionalKeyForButton(event->cbutton.button) != SDLK_UNKNOWN) {
            if (pressed) {
                g_lastDirectionalKey = key;
            } else if (g_lastDirectionalKey == key) {
                g_lastDirectionalKey = SDLK_UNKNOWN;
            }
        }
        rewriteAsKeyboardEvent(*event, pressed ? SDL_KEYDOWN : SDL_KEYUP, key);
        return 1;
    }

    if (event->type == SDL_CONTROLLERAXISMOTION) {
        if (event->caxis.which != g_controllerInstance ||
            (event->caxis.axis != SDL_CONTROLLER_AXIS_LEFTX &&
                event->caxis.axis != SDL_CONTROLLER_AXIS_LEFTY)) {
            return 0;
        }

        int& axisState = event->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ?
            g_leftXAxisState : g_leftYAxisState;
        const int previousState = axisState;
        const int requestedState = nextAxisState(event->caxis.value, previousState);
        if (requestedState == previousState) {
            return 0;
        }

        if (previousState != 0) {
            const SDL_Keycode releasedKey = keyForAxisState(
                static_cast<SDL_GameControllerAxis>(event->caxis.axis), previousState);
            // A direct negative-to-positive transition first releases the old
            // direction. The physical stick normally emits another motion event
            // immediately afterwards, which then presses the new direction.
            axisState = 0;
            if (g_lastDirectionalKey == releasedKey) {
                g_lastDirectionalKey = SDLK_UNKNOWN;
            }
            rewriteAsKeyboardEvent(*event, SDL_KEYUP, releasedKey);
            return 1;
        }

        if (requestedState != 0) {
            axisState = requestedState;
            const SDL_Keycode pressedKey = keyForAxisState(
                static_cast<SDL_GameControllerAxis>(event->caxis.axis), requestedState);
            g_lastDirectionalKey = pressedKey;
            rewriteAsKeyboardEvent(*event, SDL_KEYDOWN, pressedKey);
            return 1;
        }
        return 0;
    }

    return 1;
}

void initializeGamepadBridge() {
    if (g_gamepadBridgeInitialized) {
        return;
    }
    g_gamepadBridgeInitialized = true;

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        Logger::warn(
            std::string("Gamepad support disabled: ") + SDL_GetError(),
            __func__);
        return;
    }

    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_SetEventFilter(gamepadEventFilter, nullptr);
    openFirstAvailableController();
}

} // namespace

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
    // runApplication calls this once after SDL has been initialized. Hooking the
    // bridge here keeps controller handling inside the input layer and lets the
    // existing keyboard paths drive menus, pause, options and gameplay unchanged.
    initializeGamepadBridge();

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
