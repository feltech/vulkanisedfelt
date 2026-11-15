// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./managed_handles.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <memory>
#include <thread>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/reader_io.hpp>

#include "../backend/mock_api.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "../sdl/managed_handles.hpp"

namespace vulkandemo::vulkan
{
TEST_CASE("[impure] surface owners retain parents until final main-thread destruction")
{
    backend::MockApi api;
    sdl::detail::TestRuntime runtime{api};
    std::thread::id const main_thread = std::this_thread::get_id();
    char instance_storage{};
    char window_storage{};
    char surface_storage{};
    VkInstance instance_handle = reinterpret_cast<VkInstance>(&instance_storage);
    SDL_Window * window_handle = reinterpret_cast<SDL_Window *>(&window_storage);
    VkSurfaceKHR surface_handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    int cleanup_count = 0;
    std::weak_ptr<VkInstance_T> instance_observer;
    std::weak_ptr<SDL_Window> window_observer;
    REQUIRE_CALL(api, vk_destroy_surface_khr(instance_handle, surface_handle, nullptr))
        .WITH(std::this_thread::get_id() == main_thread)
        .LR_SIDE_EFFECT(CHECK_FALSE(instance_observer.expired()))
        .LR_SIDE_EFFECT(CHECK_FALSE(window_observer.expired()))
        .LR_SIDE_EFFECT(++cleanup_count);
    REQUIRE_CALL(api, sdl_destroy_window(window_handle))
        .WITH(std::this_thread::get_id() == main_thread)
        .LR_SIDE_EFFECT(CHECK(cleanup_count > 0))
        .LR_SIDE_EFFECT(++cleanup_count);
    REQUIRE_CALL(api, vk_destroy_instance(instance_handle, nullptr))
        .WITH(std::this_thread::get_id() == main_thread)
        .LR_SIDE_EFFECT(CHECK(cleanup_count > 0))
        .LR_SIDE_EFFECT(++cleanup_count);
    instance_ptr instance = make_instance_ptr(instance_handle, api);
    sdl::window_ptr window =
        sdl::make_window_ptr(window_handle, api, {}, runtime.environment().main_context);
    instance_observer = instance;
    window_observer = window;
    surface_ptr surface = make_surface_ptr(
        surface_handle,
        std::move(instance),
        std::move(window),
        api,
        runtime.environment().main_context);
    std::weak_ptr<surface_ptr::element_type> surface_observer = surface;

    // Copies share one Vulkan surface and keep both parents alive.
    surface_ptr retained = surface;
    surface.reset();

    CHECK_EQ(retained.get(), surface_handle);
    CHECK_EQ(cleanup_count, 0);
    CHECK_FALSE(instance_observer.expired());
    CHECK_FALSE(window_observer.expired());

    // Even weak surface observers must release parent captures after cleanup.
    retained.reset();

    CHECK_EQ(cleanup_count, 3);
    CHECK(surface_observer.expired());
    CHECK(instance_observer.expired());
    CHECK(window_observer.expired());
}

TEST_CASE("[impure] final worker surface release waits for ordered main-thread cleanup")
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    namespace reader_io = fnfelt::monad::reader_io;
    using environment_type = sdl::detail::TestState<backend::MockApi>;
    backend::MockApi api;
    sdl::detail::TestRuntime runtime{api};
    std::thread::id const main_thread = std::this_thread::get_id();
    char instance_storage{};
    char window_storage{};
    char surface_storage{};
    VkInstance instance_handle = reinterpret_cast<VkInstance>(&instance_storage);
    SDL_Window * window_handle = reinterpret_cast<SDL_Window *>(&window_storage);
    VkSurfaceKHR surface_handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    int cleanup_count = 0;
    REQUIRE_CALL(api, vk_destroy_surface_khr(instance_handle, surface_handle, nullptr))
        .WITH(std::this_thread::get_id() == main_thread)
        .LR_SIDE_EFFECT(++cleanup_count);
    REQUIRE_CALL(api, sdl_destroy_window(window_handle))
        .WITH(std::this_thread::get_id() == main_thread)
        .LR_SIDE_EFFECT(CHECK(cleanup_count > 0))
        .LR_SIDE_EFFECT(++cleanup_count);
    REQUIRE_CALL(api, vk_destroy_instance(instance_handle, nullptr))
        .WITH(std::this_thread::get_id() == main_thread)
        .LR_SIDE_EFFECT(CHECK(cleanup_count > 0))
        .LR_SIDE_EFFECT(++cleanup_count);
    surface_ptr surface = make_surface_ptr(
        surface_handle,
        make_instance_ptr(instance_handle, api),
        sdl::make_window_ptr(window_handle, api, {}, runtime.environment().main_context),
        api,
        runtime.environment().main_context);
    std::weak_ptr<surface_ptr::element_type> observer = surface;
    auto release = reader_io::create<"release_surface"_ss, environment_type>(
        [&](environment_type)
        {
            return io::create<"release_surface_owner"_ss>(
                [&]
                {
                    CHECK(std::this_thread::get_id() != main_thread);
                    surface.reset();
                    return cleanup_count;
                });
        });

    // The releasing worker resumes only after surface and parent cleanup has completed.
    auto released = runtime.run(release);

    CHECK_EQ(released, 3);
    CHECK(observer.expired());
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
