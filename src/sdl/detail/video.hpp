// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/video.hpp
 *
 * Deferred SDL video acquisition and main-thread ownership.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <SDL.h>
#include <fnfelt/monad/io.hpp>
#include <fnfelt/result.hpp>

#include "../errors.hpp"
#include "../main_thread.hpp"
#include "../resources.hpp"
#include "./errors.hpp"
#include "./main_thread.hpp"

namespace vulkandemo::sdl
{
/** Commands and resource storage needed to acquire SDL video. */
template <class TEnvironment>
concept sdl_initialize_video_environment =
    main_thread_environment<TEnvironment> && requires(TEnvironment environment, Uint32 flags)
{
    {environment.sdl_resources.get()}->std::same_as<SdlResources &>;
    requires std::convertible_to<decltype((environment.sdl_resources)), SdlResources &>;
    {environment.api.get().sdl_init_sub_system(flags)}->std::same_as<int>;
    {environment.api.get().sdl_quit_sub_system(flags)}->std::same_as<void>;
};
}  // namespace vulkandemo::sdl

namespace vulkandemo::sdl::detail
{
/**
 * Acquires video with cleanup retained on main.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed SDL implementation.
 * @param resources Main-owned resource anchors.
 * @return Successful initialization or an owned SDL failure.
 */
template <class TApi>
auto initialize_video(std::reference_wrapper<TApi> api, SdlResources & resources)
    -> sdl_result<void>
{
    namespace result = fnfelt::result;
    using result_type = sdl_result<void>;
    contract_assert(is_main_thread());
    auto & owner = resources.video_owner;
    contract_assert(!owner);
    if (api.get().sdl_init_sub_system(SDL_INIT_VIDEO) != 0)
    {
        return result::error<result_type>(
            last_failure(api, FallibleSdlOperation::initialize_video));
    }
    // Shared cleanup retains the initialized subsystem until its final main-thread release.
    owner = make_deferred(
        resource_cleanup{[&api = api.get()] noexcept
                         {
                             contract_assert(is_main_thread());
                             api.sdl_quit_sub_system(SDL_INIT_VIDEO);
                         }});
    return result::success<result_type>();
}

/**
 * Produces main-thread video-acquisition IO from an execution environment.
 *
 * @tparam TEnvironment Execution environment type.
 * @param environment Borrowed execution capabilities.
 * @return Deferred video acquisition.
 */
template <class TEnvironment>
constexpr auto io_initialize_video(TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;

    // Callable providing video ownership in the main-thread holder.
    auto initialize =
        std::bind_front(initialize_video<api_type>, environment.api, environment.sdl_resources);

    // IO providing successful video initialization or its failure on main.
    return io::on<"sdl_initialize_video_on_main"_ss>(
        environment.main_context, io::create<"sdl_initialize_video"_ss>(std::move(initialize)));
}
}  // namespace vulkandemo::sdl::detail
