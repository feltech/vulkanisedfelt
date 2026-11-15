// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/vulkan_surface.hpp
 *
 * Main-thread acquisition of SDL Vulkan presentation surfaces.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <SDL.h>
#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/result.hpp>
#include <libfork/core/ext/context.hpp>

#include "../../execution/main_thread.hpp"
#include "../../vulkan/managed_handles.hpp"
#include "../errors.hpp"
#include "../managed_handles_types.hpp"
#include "./errors.hpp"
#include "./main_thread.hpp"

namespace vulkandemo::sdl::detail
{
/** Commands and dispatch needed to acquire and release a presentation surface. */
template <class TEnvironment>
concept sdl_vulkan_create_surface_environment = main_thread_environment<TEnvironment> &&
    requires(TEnvironment environment,
             SDL_Window * window,
             VkInstance instance,
             VkSurfaceKHR * surface,
             VkAllocationCallbacks const * allocator)
{
    {environment.api.get().sdl_vulkan_create_surface(window, instance, surface)}
        ->std::same_as<SDL_bool>;
    {environment.api.get().vk_destroy_surface_khr(instance, *surface, allocator)}
        ->std::same_as<void>;
};

/**
 * Acquires a surface retaining its window and instance.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param window Owned Vulkan-capable window.
 * @param instance Owned instance with SDL's required extensions enabled.
 * @param main_context Serviced main-thread context alive through final release.
 * @return Shared surface ownership or the immediate SDL failure.
 */
template <class TApi>
auto create_surface(
    std::reference_wrapper<TApi> api,
    window_ptr window,
    vulkan::instance_ptr instance,
    lf::worker_context * main_context)
    -> sdl_result<vulkan::surface_ptr> pre(execution::is_main_thread()) pre(window != nullptr)
        pre(instance != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = sdl_result<vulkan::surface_ptr>;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (api.get().sdl_vulkan_create_surface(window.get(), instance.get(), &surface) == SDL_FALSE)
    {
        return result::error<result_type>(
            last_failure(api, FallibleSdlOperation::create_vulkan_surface));
    }
    contract_assert(surface != VK_NULL_HANDLE);
    return result::success<result_type>(vulkan::make_surface_ptr(
        surface, std::move(instance), std::move(window), api.get(), main_context));
}

/**
 * Provides main-thread surface acquisition from an execution environment.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param window Owned Vulkan-capable window.
 * @param instance Owned instance with SDL's required extensions enabled.
 * @param environment Borrowed command implementation and main-thread context.
 * @return IO producing surface ownership or an SDL failure.
 */
template <class TEnvironment>
constexpr auto io_create_surface(
    window_ptr window, vulkan::instance_ptr instance, TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;

    // Callable providing surface ownership while both parents remain alive.
    auto acquire_surface = std::bind_front(
        create_surface<api_type>,
        environment.api,
        std::move(window),
        std::move(instance),
        environment.main_context);

    // IO providing main-thread creation and immediate capture of SDL errors.
    return io::on<"sdl_surface_on_main"_ss>(
        environment.main_context,
        io::create<"sdl_vulkan_create_surface"_ss>(std::move(acquire_surface)));
}
}  // namespace vulkandemo::sdl::detail
