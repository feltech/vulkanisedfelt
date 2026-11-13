// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/window.hpp
 *
 * Main-thread SDL window acquisition and immutable ownership updates.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <SDL_video.h>
#include <fnfelt/monad/io.hpp>
#include <fnfelt/result.hpp>
#include <immer/box.hpp>

#include "../../execution/main_thread.hpp"
#include "../errors.hpp"
#include "../managed_handles.hpp"
#include "../managed_handles_types.hpp"
#include "../resources.hpp"
#include "../window_types.hpp"
#include "./errors.hpp"
#include "./main_thread.hpp"

namespace vulkandemo::sdl::detail
{
/** Owned state and commands needed to acquire an SDL window. */
template <class TState>
concept sdl_create_window_state = std::same_as<TState, std::remove_cvref_t<TState>> &&
    requires(TState const & state,
             char const * title,
             int position,
             int size,
             Uint32 flags,
             SDL_Window * window)
{
    typename TState::value_type;
    typename TState::memory_policy;
    requires(
        std::same_as<
            TState,
            immer::box<typename TState::value_type, typename TState::memory_policy>>);
    requires std::copyable<typename TState::value_type>;
    requires main_thread_environment<typename TState::value_type>;
    {state.get().sdl_resources}->std::same_as<SdlResources const &>;
    {state.update(std::identity{})}->std::same_as<TState>;
    {state.get().api.get().sdl_create_window(title, position, position, size, size, flags)}
        ->std::same_as<SDL_Window *>;
    {state.get().api.get().sdl_destroy_window(window)}->std::same_as<void>;
};
/**
 * Creates shared window ownership with ordered main-thread cleanup.
 *
 * @tparam TApi Command implementation type.
 * @param options Owned window configuration.
 * @param api Borrowed SDL implementation.
 * @param video Video subsystem lifetime.
 * @param current Existing window ownership.
 * @param main_context Serviced main-thread context alive through cleanup.
 * @return Shared window ownership or owned SDL failure.
 */
template <class TApi>
auto create_window(
    WindowOptions const & options,
    std::reference_wrapper<TApi> api,
    video_keepalive video,
    window_ptr const & current,
    lf::worker_context * main_context) -> sdl_result<window_ptr> pre(execution::is_main_thread())
    pre(video != nullptr && !current)
{
    namespace result = fnfelt::result;
    using result_type = sdl_result<window_ptr>;

    SDL_Window * handle = api.get().sdl_create_window(
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
    return result::success<result_type>(
        make_window_ptr(handle, api.get(), std::move(video), main_context));
}

/**
 * Supplies main-thread window acquisition with its execution capabilities.
 *
 * @tparam TState Execution state type.
 * @param options Owned window configuration.
 * @param environment Borrowed API, resources and main context.
 * @return Deferred window acquisition.
 */
template <class TState>
constexpr auto create_window_reader_io_action(
    WindowOptions const & options, TState const & environment)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    typename TState::value_type const & snapshot = environment.get();
    using api_type = typename std::remove_cvref_t<decltype(snapshot.api)>::type;

    // Callable owning its video dependency while acquisition is deferred.
    auto acquire_window = std::bind_front(
        create_window<api_type>,
        options,
        snapshot.api,
        snapshot.sdl_resources.video_owner,
        snapshot.sdl_resources.window_owner,
        snapshot.main_context);

    // IO providing shared window ownership after main-thread acquisition.
    return io::on<"sdl_create_window_on_main"_ss>(
        snapshot.main_context, io::create<"sdl_create_window"_ss>(std::move(acquire_window)));
}

/**
 * Retains an acquired window in a new resource snapshot.
 *
 * @tparam TState Owned execution state type.
 * @param owner Acquired window ownership.
 * @param state Previous execution state.
 * @return Updated execution state.
 */
template <class TState>
constexpr TState store_window(window_ptr const & owner, TState state)
{
    return std::move(state).update(
        [&owner](typename TState::value_type snapshot)
        {
            snapshot.sdl_resources.window_owner = owner;
            return snapshot;
        });
}

/**
 * Provides a borrowed view of retained window ownership.
 *
 * @param owner Retained window ownership.
 * @return Borrowed window handle.
 */
inline auto window_handle(window_ptr const & owner) noexcept -> SDL_Window *
{
    return owner.get();
}
}  // namespace vulkandemo::sdl::detail
