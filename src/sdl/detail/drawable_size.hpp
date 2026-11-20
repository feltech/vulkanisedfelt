// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/drawable_size.hpp
 *
 * Main-thread drawable-size snapshots.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <SDL.h>
#include <fnfelt/monad/io.hpp>
#include <libfork/core/ext/context.hpp>

#include "../../backend/detail/api.hpp"
#include "../../execution/main_thread.hpp"
#include "../drawable_size_types.hpp"
#include "../managed_handles_types.hpp"

namespace vulkandemo::sdl::detail
{
/** Commands needed to inspect a window's drawable dimensions. */
template <class TEnvironment>
concept sdl_vulkan_get_drawable_size_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment environment, SDL_Window * window, int * width, int * height)
{
    {environment.main_context}->std::convertible_to<lf::worker_context *>;
    {environment.api.get().sdl_vulkan_get_drawable_size(window, width, height)}->std::same_as<void>;
};

/**
 * Retrieves the current drawable dimensions on main.
 *
 * @param api Borrowed command implementation.
 * @param window Retained Vulkan-capable window.
 * @return Nonnegative pixel dimensions.
 */
template <class TApi>
auto get_drawable_size(std::reference_wrapper<TApi> api, window_ptr window) -> DrawableSize
    pre(execution::is_main_thread()) pre(window != nullptr)
        post(size : value_of(size.width) >= 0 && value_of(size.height) >= 0)
{
    int width = 0;
    int height = 0;
    api.get().sdl_vulkan_get_drawable_size(window.get(), &width, &height);
    return DrawableSize{drawable_width{width}, drawable_height{height}};
}

/**
 * Provides a main-thread drawable-size query.
 *
 * @param window Retained Vulkan-capable window.
 * @param environment Borrowed commands and serviced main context.
 * @return IO providing live pixel dimensions.
 */
template <class TEnvironment>
constexpr auto io_get_drawable_size(window_ptr window, TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing current pixels while retaining the window and its video parent.
    auto query_size =
        std::bind_front(get_drawable_size<api_type>, environment.api, std::move(window));

    // IO providing the snapshot from SDL's main thread.
    return io::on<"sdl_drawable_size_on_main"_ss>(
        environment.main_context,
        io::create<"sdl_vulkan_get_drawable_size"_ss>(std::move(query_size)));
}
}  // namespace vulkandemo::sdl::detail
