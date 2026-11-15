// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/vulkan_surface.hpp
 *
 * Deferred creation of owning SDL Vulkan presentation surfaces.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "../vulkan/managed_handles_types.hpp"
#include "./managed_handles_types.hpp"
#include "detail/vulkan_surface.hpp"

namespace vulkandemo::sdl
{
/**
 * Plans a presentation surface retaining its window and instance.
 *
 * The window must have SDL_WINDOW_VULKAN and the instance must enable SDL's
 * required extensions. Creation and final cleanup run on main. The borrowed API
 * and serviced main_context must remain alive through the final owner's release.
 * The environment remains unchanged; get() provides the native surface handle.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param window Owned Vulkan-capable window.
 * @param instance Owned instance with the required extensions enabled.
 * @return ReaderIO producing shared surface ownership or an owned SDL failure.
 */
template <detail::sdl_vulkan_create_surface_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_sdl_vulkan_create_surface(
    window_ptr window, vulkan::instance_ptr instance)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory providing acquisition IO with both parent lifetimes retained.
    auto io_create_surface = std::bind_front(
        detail::io_create_surface<TEnvironment>, std::move(window), std::move(instance));

    // ReaderIO providing a surface whose shared ownership includes both parents.
    return reader_io::create<"sdl_vulkan_create_surface"_ss, TEnvironment>(
        std::move(io_create_surface));
}
}  // namespace vulkandemo::sdl
