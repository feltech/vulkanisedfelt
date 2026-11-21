// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_instance.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <variant>

#include <SDL.h>
#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>
#include <immer/box.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "../sdl/errors.hpp"
#include "../sdl/managed_handles_types.hpp"
#include "../sdl/video.hpp"
#include "../sdl/window.hpp"
#include "./errors.hpp"
#include "./extension_types.hpp"
#include "./managed_handles_types.hpp"
#include "./presentation_instance_configuration.hpp"
#include "./presentation_instance_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
VKAPI_ATTR VkResult VKAPI_CALL presentation_instance_version(std::uint32_t * version)
{
    *version = VK_API_VERSION_1_1;
    return VK_SUCCESS;
}

auto presentation_instance_properties(bool has_paths = true) -> immer::array<VkExtensionProperties>
{
    auto names =
        immer::array<std::string_view>{
            "VK_TEST_platform",
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME}
            .transient();
    if (has_paths)
    {
        names.push_back("VK_KHR_surface_maintenance1");
        names.push_back("VK_EXT_surface_maintenance1");
    }
    auto properties = immer::array<VkExtensionProperties>{}.transient();
    for (std::string_view name : names)
    {
        VkExtensionProperties property{};
        std::ranges::copy(name, property.extensionName);
        properties.push_back(property);
    }
    return std::move(properties).persistent();
}
}  // namespace

TEST_CASE("[impure] presentation instance creation enables selected names and retains owned facts")
{
    backend::MockApi api;
    sdl::detail::TestRuntime runtime{api};
    using environment_type = sdl::detail::TestState<backend::MockApi>;
    char window_storage{}, instance_storage{};
    SDL_Window * window_handle = reinterpret_cast<SDL_Window *>(&window_storage);
    VkInstance instance_handle = reinterpret_cast<VkInstance>(&instance_storage);
    std::atomic<bool> is_window_destroyed{false};
    bool is_instance_destroyed = false;
    sdl::window_ptr window{window_handle, [&](auto) { is_window_destroyed.store(true); }};
    immer::array<VkExtensionProperties> properties = presentation_instance_properties();
    PresentationInstance snapshot;
    REQUIRE_CALL(api, vk_destroy_instance(instance_handle, nullptr))
        .LR_SIDE_EFFECT(is_instance_destroyed = true);
    {
        // Discovery and instance creation remain deferred after the caller releases its window.
        auto plan = reader_io_create_presentation_instance<environment_type>(window);
        window.reset();
        CHECK_FALSE(is_window_destroyed.load());
        REQUIRE_CALL(
            api, sdl_vulkan_get_instance_extensions(window_handle, trompeloeil::_, nullptr))
            .SIDE_EFFECT(*_2 = 2)
            .RETURN(SDL_TRUE);
        REQUIRE_CALL(
            api, sdl_vulkan_get_instance_extensions(window_handle, trompeloeil::_, trompeloeil::_))
            .WITH(_3 != nullptr)
            .SIDE_EFFECT(_3[0] = "VK_TEST_platform")
            .SIDE_EFFECT(_3[1] = VK_KHR_SURFACE_EXTENSION_NAME)
            .RETURN(SDL_TRUE);
        REQUIRE_CALL(
            api, vk_enumerate_instance_extension_properties(nullptr, trompeloeil::_, nullptr))
            .LR_SIDE_EFFECT(*_2 = static_cast<std::uint32_t>(properties.size()))
            .RETURN(VK_SUCCESS);
        REQUIRE_CALL(
            api,
            vk_enumerate_instance_extension_properties(nullptr, trompeloeil::_, trompeloeil::_))
            .WITH(_3 != nullptr)
            .LR_SIDE_EFFECT(std::ranges::copy(properties, _3))
            .RETURN(VK_SUCCESS);
        REQUIRE_CALL(api, vk_get_instance_proc_addr(nullptr, trompeloeil::_))
            .WITH(std::string_view{_2} == "vkEnumerateInstanceVersion")
            .RETURN(reinterpret_cast<PFN_vkVoidFunction>(&presentation_instance_version));
        REQUIRE_CALL(api, vk_create_instance(trompeloeil::_, nullptr, trompeloeil::_))
            .WITH(_1->enabledExtensionCount == 5)
            .SIDE_EFFECT(
                CHECK_EQ(std::string_view{_1->ppEnabledExtensionNames[0]}, "VK_TEST_platform"))
            .SIDE_EFFECT(CHECK_EQ(
                std::string_view{_1->ppEnabledExtensionNames[1]}, VK_KHR_SURFACE_EXTENSION_NAME))
            .SIDE_EFFECT(CHECK_EQ(
                std::string_view{_1->ppEnabledExtensionNames[2]},
                VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME))
            .SIDE_EFFECT(CHECK_EQ(
                std::string_view{_1->ppEnabledExtensionNames[3]}, "VK_KHR_surface_maintenance1"))
            .SIDE_EFFECT(CHECK_EQ(
                std::string_view{_1->ppEnabledExtensionNames[4]}, "VK_EXT_surface_maintenance1"))
            .SIDE_EFFECT(CHECK_EQ(_1->pApplicationInfo->apiVersion, VK_API_VERSION_1_1))
            .LR_SIDE_EFFECT(*_3 = instance_handle)
            .RETURN(VK_SUCCESS);

        presentation_instance_result outcome = runtime.run(plan);

        REQUIRE(fnfelt::result::is_success(outcome));
        snapshot = fnfelt::result::get_success(std::move(outcome));
        CHECK_EQ(snapshot.instance.get(), instance_handle);
        CHECK_FALSE(is_instance_destroyed);
    }
    // The enabled names are independent of the discovery window and plan.
    CHECK(is_window_destroyed.load());
    REQUIRE_EQ(snapshot.enabled_extensions.size(), 5);
    CHECK_EQ(value_of(snapshot.enabled_extensions[4]).view(), "VK_EXT_surface_maintenance1");
    instance_ptr alias = snapshot.instance;
    snapshot = {};
    CHECK_FALSE(is_instance_destroyed);

    alias.reset();

    CHECK(is_instance_destroyed);
}

TEST_CASE(
    "[impure] presentation instance creation bypasses acquisition on unsupported prerequisites")
{
    backend::MockApi api;
    sdl::detail::TestRuntime runtime{api};
    using environment_type = sdl::detail::TestState<backend::MockApi>;
    char window_storage{};
    sdl::window_ptr window{reinterpret_cast<SDL_Window *>(&window_storage), [](auto) {}};
    bool has_paths = false;
    SUBCASE("neither maintenance1 family is available") {}
    SUBCASE("instance creation fails after successful discovery")
    {
        has_paths = true;
    }
    immer::array<VkExtensionProperties> properties = presentation_instance_properties(has_paths);
    REQUIRE_CALL(api, sdl_vulkan_get_instance_extensions(window.get(), trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_2 = 0)
        .RETURN(SDL_TRUE);
    REQUIRE_CALL(api, vk_enumerate_instance_extension_properties(nullptr, trompeloeil::_, nullptr))
        .LR_SIDE_EFFECT(*_2 = static_cast<std::uint32_t>(properties.size()))
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api, vk_enumerate_instance_extension_properties(nullptr, trompeloeil::_, trompeloeil::_))
        .WITH(_3 != nullptr)
        .LR_SIDE_EFFECT(std::ranges::copy(properties, _3))
        .RETURN(VK_SUCCESS);
    std::unique_ptr<trompeloeil::expectation> version_query;
    std::unique_ptr<trompeloeil::expectation> instance_creation;
    if (has_paths)
    {
        version_query =
            NAMED_REQUIRE_CALL(api, vk_get_instance_proc_addr(nullptr, trompeloeil::_))
                .RETURN(reinterpret_cast<PFN_vkVoidFunction>(&presentation_instance_version));
        instance_creation =
            NAMED_REQUIRE_CALL(api, vk_create_instance(trompeloeil::_, nullptr, trompeloeil::_))
                .RETURN(VK_ERROR_OUT_OF_HOST_MEMORY);
    }

    presentation_instance_result outcome =
        runtime.run(reader_io_create_presentation_instance<environment_type>(window));

    REQUIRE(fnfelt::result::is_error(outcome));
    if (has_paths)
    {
        VulkanFailure failure =
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(outcome)).cause;
        CHECK_EQ(failure.operation, FallibleVulkanOperation::create_instance);
        CHECK_EQ(failure.result, VK_ERROR_OUT_OF_HOST_MEMORY);
    }
    else
    {
        PresentationInstanceFailure failure =
            std::get<PresentationInstanceFailure>(fnfelt::result::get_error(outcome));
        CHECK_EQ(failure.reason, PresentationInstanceFailureReason::no_maintenance1_path);
    }
}

TEST_CASE(
    "[integration][windowed] presentation instance enables advertised maintenance1 prerequisites")
{
    backend::Api api;
    using environment_type = sdl::detail::TestState<backend::Api>;
    using state_type = immer::box<environment_type>;
    sdl::detail::TestRuntime runtime{api};
    sdl::sdl_result<void> video = runtime.run(sdl::state_io_sdl_initialize_video<state_type>());
    REQUIRE(fnfelt::result::is_success(video));
    sdl::WindowOptions options{
        .width = sdl::window_width{64},
        .height = sdl::window_height{48},
        .flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)};
    sdl::sdl_result<SDL_Window *> window =
        runtime.run(sdl::state_io_sdl_create_window<state_type>(options));
    REQUIRE(fnfelt::result::is_success(window));
    sdl::window_ptr window_owner = runtime.resources().window_owner;

    renderer_result<immer::array<extension_name>> extension_names = runtime.run(
        reader_io_discover_presentation_instance_configuration<environment_type>(window_owner));
    presentation_instance_result outcome =
        runtime.run(reader_io_create_presentation_instance<environment_type>(window_owner));

    // Missing support is an expected capability outcome, not cancellation or a skipped test.
    if (fnfelt::result::is_error(extension_names))
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        REQUIRE(
            std::holds_alternative<PresentationInstanceFailure>(
                fnfelt::result::get_error(extension_names)));
        REQUIRE(
            std::holds_alternative<PresentationInstanceFailure>(
                fnfelt::result::get_error(outcome)));
        PresentationInstanceFailure expected =
            std::get<PresentationInstanceFailure>(fnfelt::result::get_error(extension_names));
        PresentationInstanceFailure failure =
            std::get<PresentationInstanceFailure>(fnfelt::result::get_error(outcome));
        CHECK_EQ(failure.reason, expected.reason);
        CHECK_EQ(failure.missing_extensions, expected.missing_extensions);
        return;
    }
    REQUIRE(fnfelt::result::is_success(extension_names));
    REQUIRE(fnfelt::result::is_success(outcome));
    PresentationInstance snapshot = fnfelt::result::get_success(std::move(outcome));
    immer::array<extension_name> selected = fnfelt::result::get_success(std::move(extension_names));
    CHECK(snapshot.instance.get() != VK_NULL_HANDLE);
    CHECK_EQ(snapshot.enabled_extensions, selected);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
