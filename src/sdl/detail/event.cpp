// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./event.hpp"

namespace vulkandemo::sdl::detail
{
auto has_pointer_free_payload(Uint32 type) noexcept -> bool
{
    switch (type)
    {
        case SDL_QUIT:
        case SDL_APP_TERMINATING:
        case SDL_APP_LOWMEMORY:
        case SDL_APP_WILLENTERBACKGROUND:
        case SDL_APP_DIDENTERBACKGROUND:
        case SDL_APP_WILLENTERFOREGROUND:
        case SDL_APP_DIDENTERFOREGROUND:
        case SDL_LOCALECHANGED:
        case SDL_DISPLAYEVENT:
        case SDL_WINDOWEVENT:
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_TEXTEDITING:
        case SDL_TEXTINPUT:
        case SDL_KEYMAPCHANGED:
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
        case SDL_JOYAXISMOTION:
        case SDL_JOYBALLMOTION:
        case SDL_JOYHATMOTION:
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
        case SDL_JOYDEVICEADDED:
        case SDL_JOYDEVICEREMOVED:
        case SDL_JOYBATTERYUPDATED:
        case SDL_CONTROLLERAXISMOTION:
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
        case SDL_CONTROLLERDEVICEREMAPPED:
        case SDL_CONTROLLERTOUCHPADDOWN:
        case SDL_CONTROLLERTOUCHPADMOTION:
        case SDL_CONTROLLERTOUCHPADUP:
        case SDL_CONTROLLERSENSORUPDATE:
        case SDL_CONTROLLERSTEAMHANDLEUPDATED:
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
        case SDL_DOLLARGESTURE:
        case SDL_DOLLARRECORD:
        case SDL_MULTIGESTURE:
        case SDL_CLIPBOARDUPDATE:
        case SDL_AUDIODEVICEADDED:
        case SDL_AUDIODEVICEREMOVED:
        case SDL_SENSORUPDATE:
        case SDL_RENDER_TARGETS_RESET:
        case SDL_RENDER_DEVICE_RESET:
            return true;
        default:
            return false;
    }
}
}  // namespace vulkandemo::sdl::detail
