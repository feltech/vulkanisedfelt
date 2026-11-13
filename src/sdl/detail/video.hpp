// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/video.hpp
 *
 * Deferred SDL video acquisition and immutable ownership updates.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <SDL.h>
#include <fnfelt/monad/io.hpp>
#include <fnfelt/result.hpp>
#include <immer/box.hpp>

#include "../../execution/main_thread.hpp"
#include "../../execution/managed_resources.hpp"
#include "../errors.hpp"
#include "../managed_handles_types.hpp"
#include "../resources.hpp"
#include "./errors.hpp"
#include "./main_thread.hpp"

namespace vulkandemo::sdl::detail
{
/** Owned state and commands needed to acquire SDL video. */
template <class TState>
concept sdl_initialize_video_state = std::same_as<TState, std::remove_cvref_t<TState>> &&
    requires(TState const & state, Uint32 flags)
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
    {state.get().api.get().sdl_init_sub_system(flags)}->std::same_as<int>;
    {state.get().api.get().sdl_quit_sub_system(flags)}->std::same_as<void>;
};

/**
 * Releases the video subsystem on main.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed SDL implementation.
 */
template <class TApi>
void quit_video(std::reference_wrapper<TApi> api) noexcept pre(execution::is_main_thread())
{
    api.get().sdl_quit_sub_system(SDL_INIT_VIDEO);
}

/**
 * Acquires shared video ownership with main-thread cleanup.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed SDL implementation.
 * @param current Existing video ownership.
 * @param main_context Serviced main-thread context alive through cleanup.
 * @return Shared video ownership or an owned SDL failure.
 */
template <class TApi>
auto initialize_video(
    std::reference_wrapper<TApi> api,
    video_keepalive const & current,
    lf::worker_context * main_context)
    -> sdl_result<video_keepalive> pre(execution::is_main_thread()) pre(!current)
{
    namespace result = fnfelt::result;
    using result_type = sdl_result<video_keepalive>;
    if (api.get().sdl_init_sub_system(SDL_INIT_VIDEO) != 0)
    {
        return result::error<result_type>(
            last_failure(api, FallibleSdlOperation::initialize_video));
    }
    // Shared ownership permits final release on a worker while cleanup still executes on main.
    video_keepalive owner =
        execution::make_main_thread_deferred(main_context, std::bind_front(quit_video<TApi>, api));
    return result::success<result_type>(std::move(owner));
}

/**
 * Produces main-thread video-acquisition IO from an execution environment.
 *
 * @tparam TState Execution state type.
 * @param environment Borrowed execution capabilities.
 * @return Deferred video acquisition.
 */
template <class TState>
constexpr auto initialize_video_reader_io_action(TState const & environment)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    typename TState::value_type const & snapshot = environment.get();
    using api_type = typename std::remove_cvref_t<decltype(snapshot.api)>::type;

    // Callable providing shared ownership without changing the environment.
    auto initialize = std::bind_front(
        initialize_video<api_type>,
        snapshot.api,
        snapshot.sdl_resources.video_owner,
        snapshot.main_context);

    // IO providing successful video initialization or its failure on main.
    return io::on<"sdl_initialize_video_on_main"_ss>(
        snapshot.main_context, io::create<"sdl_initialize_video"_ss>(std::move(initialize)));
}

/**
 * Retains acquired video in a new resource snapshot.
 *
 * @tparam TState Owned execution state type.
 * @param owner Acquired video ownership.
 * @param state Previous execution state.
 * @return Updated execution state.
 */
template <class TState>
constexpr TState store_video(video_keepalive const & owner, TState state)
{
    return std::move(state).update(
        [&owner](typename TState::value_type snapshot)
        {
            snapshot.sdl_resources.video_owner = owner;
            return snapshot;
        });
}
}  // namespace vulkandemo::sdl::detail
