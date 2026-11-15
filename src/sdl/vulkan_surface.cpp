// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./vulkan_surface.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <array>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/box.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "../vulkan/extension_types.hpp"
#include "../vulkan/instance.hpp"
#include "../vulkan/managed_handles.hpp"
#include "../vulkan/renderer_errors.hpp"
#include "./instance_extensions.hpp"
#include "./managed_handles.hpp"
#include "./video.hpp"
#include "./window.hpp"
#include "./window_types.hpp"
#include "detail/test_runtime.hpp"

namespace vulkandemo::sdl
{
namespace
{
using mock_environment = detail::TestState<backend::MockApi>;
}  // namespace

TEST_CASE("[impure] SDL surface planning retains parents without creating a surface")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    char instance_storage{};
    char window_storage{};
    vulkan::instance_ptr instance{
        reinterpret_cast<VkInstance>(&instance_storage), [](auto) noexcept {}};
    window_ptr window{reinterpret_cast<SDL_Window *>(&window_storage), [](auto) noexcept {}};
    std::weak_ptr<VkInstance_T> instance_observer = instance;
    std::weak_ptr<SDL_Window> window_observer = window;

    {
        // Neither planning nor binding an environment performs API effects.
        auto plan = reader_io_sdl_vulkan_create_surface<mock_environment>(
            std::move(window), std::move(instance));
        [[maybe_unused]] auto deferred = plan.build(runtime.environment());

        CHECK_FALSE(instance_observer.expired());
        CHECK_FALSE(window_observer.expired());
    }

    CHECK(instance_observer.expired());
    CHECK(window_observer.expired());
}

TEST_CASE("[impure] SDL surface creation returns ownership and leaves its environment unchanged")
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    std::thread::id const main_thread = std::this_thread::get_id();
    char instance_storage{};
    char window_storage{};
    char surface_storage{};
    VkInstance instance_handle = reinterpret_cast<VkInstance>(&instance_storage);
    SDL_Window * window_handle = reinterpret_cast<SDL_Window *>(&window_storage);
    VkSurfaceKHR surface_handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    bool is_surface_destroyed = false;
    REQUIRE_CALL(api, sdl_vulkan_create_surface(window_handle, instance_handle, trompeloeil::_))
        .WITH(std::this_thread::get_id() == main_thread && _3 != nullptr)
        .LR_SIDE_EFFECT(*_3 = surface_handle)
        .RETURN(SDL_TRUE);
    REQUIRE_CALL(api, vk_destroy_surface_khr(instance_handle, surface_handle, nullptr))
        .WITH(std::this_thread::get_id() == main_thread)
        .LR_SIDE_EFFECT(is_surface_destroyed = true);
    REQUIRE_CALL(api, vk_destroy_instance(instance_handle, nullptr))
        .LR_SIDE_EFFECT(CHECK(is_surface_destroyed));
    REQUIRE_CALL(api, sdl_destroy_window(window_handle))
        .LR_SIDE_EFFECT(CHECK(is_surface_destroyed));
    vulkan::instance_ptr instance = vulkan::make_instance_ptr(instance_handle, api);
    window_ptr window = make_window_ptr(window_handle, api, {}, runtime.environment().main_context);
    std::weak_ptr<VkInstance_T> instance_observer = instance;
    std::weak_ptr<SDL_Window> window_observer = window;

    // The temporary plan transfers its parent lifetimes into the acquired owner.
    sdl_result<vulkan::surface_ptr> result = runtime.run(
        reader_io::transform<"resume_surface_on_worker"_ss>(
            reader_io_sdl_vulkan_create_surface<mock_environment>(
                std::move(window), std::move(instance)),
            [main_thread](vulkan::surface_ptr surface)
            {
                CHECK(std::this_thread::get_id() != main_thread);
                return surface;
            }));

    REQUIRE(fnfelt::result::is_success(result));
    vulkan::surface_ptr & surface = fnfelt::result::get_success(result);
    CHECK_EQ(surface.get(), surface_handle);
    CHECK_FALSE(is_surface_destroyed);
    CHECK_FALSE(instance_observer.expired());
    CHECK_FALSE(window_observer.expired());
    CHECK_FALSE(runtime.resources().window_owner);
    CHECK_FALSE(runtime.resources().video_owner);

    // Releasing the surface releases both parents even while a weak observer remains.
    std::weak_ptr<vulkan::surface_ptr::element_type> observer = surface;
    surface.reset();

    CHECK(is_surface_destroyed);
    CHECK(observer.expired());
    CHECK(instance_observer.expired());
    CHECK(window_observer.expired());
}

TEST_CASE("[impure] SDL surface failure owns its immediate error without surface destruction")
{
    backend::MockApi api;
    detail::TestRuntime runtime{api};
    std::thread::id const main_thread = std::this_thread::get_id();
    char instance_storage{};
    char window_storage{};
    VkInstance instance_handle = reinterpret_cast<VkInstance>(&instance_storage);
    SDL_Window * window_handle = reinterpret_cast<SDL_Window *>(&window_storage);
    std::array message{'s', 'u', 'r', 'f', 'a', 'c', 'e', ' ', 'f', 'a', 'i', 'l', '\0'};
    trompeloeil::sequence order;
    REQUIRE_CALL(api, sdl_vulkan_create_surface(window_handle, instance_handle, trompeloeil::_))
        .WITH(std::this_thread::get_id() == main_thread)
        .IN_SEQUENCE(order)
        .RETURN(SDL_FALSE);
    REQUIRE_CALL(api, sdl_get_error())
        .WITH(std::this_thread::get_id() == main_thread)
        .IN_SEQUENCE(order)
        .RETURN(message.data());
    REQUIRE_CALL(api, vk_destroy_instance(instance_handle, nullptr));
    REQUIRE_CALL(api, sdl_destroy_window(window_handle));
    vulkan::instance_ptr instance = vulkan::make_instance_ptr(instance_handle, api);
    window_ptr window = make_window_ptr(window_handle, api, {}, runtime.environment().main_context);

    // The error is copied on main before another SDL operation can replace it.
    sdl_result<vulkan::surface_ptr> result = runtime.run(
        reader_io_sdl_vulkan_create_surface<mock_environment>(
            std::move(window), std::move(instance)));
    message[0] = 'X';

    REQUIRE(fnfelt::result::is_error(result));
    SdlFailure const & failure = fnfelt::result::get_error(result);
    CHECK(failure.operation == FallibleSdlOperation::create_vulkan_surface);
    CHECK_EQ(std::string_view{failure.message.data(), failure.message.size()}, "surface fail");
    // No surface destruction expectation: failed acquisition never creates an owner.
}

TEST_CASE("[integration][windowed] SDL creates an owning Vulkan surface for a hidden window")
{
    backend::Api api;
    using environment_type = detail::TestState<backend::Api>;
    using state_type = immer::box<environment_type>;
    detail::TestRuntime runtime{api};
    REQUIRE(fnfelt::result::is_success(runtime.run(state_io_sdl_initialize_video<state_type>())));
    WindowOptions options{
        .width = window_width{64},
        .height = window_height{48},
        .flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)};
    sdl_result<SDL_Window *> window = runtime.run(state_io_sdl_create_window<state_type>(options));
    REQUIRE(fnfelt::result::is_success(window));
    sdl_result<immer::array<vulkan::extension_name>> extensions = runtime.run(
        reader_io_sdl_vulkan_get_instance_extensions<environment_type>(
            fnfelt::result::get_success(window)));
    REQUIRE(fnfelt::result::is_success(extensions));
    vulkan::renderer_result<vulkan::instance_ptr> instance = runtime.run(
        vulkan::reader_io_vk_create_instance<environment_type>(
            fnfelt::result::get_success(extensions)));
    REQUIRE(fnfelt::result::is_success(instance));

    // The owning input couples the window's lifetime to its Vulkan surface.
    sdl_result<vulkan::surface_ptr> surface = runtime.run(
        reader_io_sdl_vulkan_create_surface<environment_type>(
            runtime.resources().window_owner, fnfelt::result::get_success(instance)));

    REQUIRE(fnfelt::result::is_success(surface));
    CHECK_NE(fnfelt::result::get_success(surface).get(), VK_NULL_HANDLE);
    // Reverse lexical destruction releases the surface before instance/window/video.
}

namespace
{
struct SurfaceApi
{
    SDL_bool sdl_vulkan_create_surface(SDL_Window *, VkInstance, VkSurfaceKHR *);
    void vk_destroy_surface_khr(VkInstance, VkSurfaceKHR, VkAllocationCallbacks const *);
    char const * sdl_get_error();
};

struct MissingDestructionApi
{
    SDL_bool sdl_vulkan_create_surface(SDL_Window *, VkInstance, VkSurfaceKHR *);
    char const * sdl_get_error();
};

static_assert(detail::sdl_vulkan_create_surface_environment<detail::TestState<SurfaceApi>>);
static_assert(
    !detail::sdl_vulkan_create_surface_environment<detail::TestState<MissingDestructionApi>>);
}  // namespace
}  // namespace vulkandemo::sdl
#endif  // DOCTEST_CONFIG_DISABLE
