// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/window.hpp
 *
 * Main-thread SDL window acquisition with ordered resource cleanup.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <SDL_video.h>
#include <fnfelt/monad/io.hpp>
#include <fnfelt/result.hpp>

#include "../errors.hpp"
#include "../main_thread.hpp"
#include "../managed_handles.hpp"
#include "../resources.hpp"
#include "../window_types.hpp"
#include "./errors.hpp"
#include "./main_thread.hpp"

namespace vulkandemo::sdl
{
/** Commands and resource storage needed to acquire an SDL window. */
template <class TEnvironment>
concept sdl_create_window_environment = main_thread_environment<TEnvironment> &&
    requires(TEnvironment environment,
             char const * title,
             int position,
             int size,
             Uint32 flags,
             SDL_Window * window)
{
    {environment.sdl_resources.get()}->std::same_as<SdlResources &>;
    requires std::convertible_to<decltype((environment.sdl_resources)), SdlResources &>;
    {environment.api.get().sdl_create_window(title, position, position, size, size, flags)}
        ->std::same_as<SDL_Window *>;
    {environment.api.get().sdl_destroy_window(window)}->std::same_as<void>;
};
}  // namespace vulkandemo::sdl

namespace vulkandemo::sdl::detail
{
/**
 * Creates a window and anchors its lifetime on main.
 *
 * @tparam TApi Command implementation type.
 * @param options Owned window configuration.
 * @param api Borrowed SDL implementation.
 * @param resources Main-owned window and video anchors.
 * @return Borrowed window or owned SDL failure.
 */
template <class TApi>
auto create_window(
    WindowOptions const & options, std::reference_wrapper<TApi> api, SdlResources & resources)
    -> sdl_result<SDL_Window *>
{
    namespace result = fnfelt::result;
    using result_type = sdl_result<SDL_Window *>;
    contract_assert(is_main_thread());
    contract_assert(resources.video_owner != nullptr && !resources.window_owner);

    auto * handle = api.get().sdl_create_window(
        options.title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        value_of(options.width),
        value_of(options.height),
        static_cast<Uint32>(options.flags));
    if (!handle)
    {
        return result::error<result_type>(last_failure(api, FallibleSdlOperation::create_window));
    }
    resources.window_owner = make_window_ptr(handle, api.get(), resources.video_owner);
    return result::success<result_type>(handle);
}

/**
 * Supplies main-thread window acquisition with its execution capabilities.
 *
 * @tparam TEnvironment Execution environment type.
 * @param options Owned window configuration.
 * @param environment Borrowed API, resources and main context.
 * @return Deferred window acquisition.
 */
template <class TEnvironment>
constexpr auto io_create_window(WindowOptions const & options, TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;

    // Callable providing a main-owned window with cleanup preceding video shutdown.
    auto acquire_window = std::bind_front(
        create_window<api_type>, options, environment.api, environment.sdl_resources);

    // IO providing borrowed window access after main-thread acquisition.
    return io::on<"sdl_create_window_on_main"_ss>(
        environment.main_context, io::create<"sdl_create_window"_ss>(std::move(acquire_window)));
}
}  // namespace vulkandemo::sdl::detail
