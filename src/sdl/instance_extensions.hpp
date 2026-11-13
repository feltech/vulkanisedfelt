// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/instance_extensions.hpp
 *
 * Deferred discovery of SDL's required Vulkan instance extensions.
 */
#pragma once

#include <functional>
#include <utility>

#include <SDL.h>
#include <fnfelt/monad/reader_io.hpp>

#include "detail/instance_extensions.hpp"

namespace vulkandemo::sdl
{
/**
 * Plans an owned snapshot of the window's required Vulkan instance extensions.
 *
 * The window must have SDL_WINDOW_VULKAN and remain alive through execution.
 * The environment borrows api through a reference wrapper and supplies main_context.
 * Keep both alive through execution. Returned names contain no trailing NUL.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param window Borrowed Vulkan-capable window.
 * @return ReaderIO producing immutable extension names or owned SDL failure.
 */
template <sdl_vulkan_get_instance_extensions_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_sdl_vulkan_get_instance_extensions(SDL_Window * window)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory providing main-thread extension-query IO for the borrowed window.
    auto io_get_instance_extensions =
        std::bind_front(detail::io_get_instance_extensions<TEnvironment>, window);

    // ReaderIO providing owned extension names independent of SDL's string storage.
    return reader_io::create<"sdl_vulkan_get_instance_extensions"_ss, TEnvironment>(
        std::move(io_get_instance_extensions));
}
}  // namespace vulkandemo::sdl
