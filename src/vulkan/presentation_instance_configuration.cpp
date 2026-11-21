// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_instance_configuration.hpp"

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

#include "../backend/mock_api.hpp"
#include "../execution/main_thread.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "../sdl/errors.hpp"
#include "../sdl/managed_handles_types.hpp"
#include "./errors.hpp"
#include "./extension_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
auto presentation_extension_properties() -> immer::array<VkExtensionProperties>
{
    auto properties = immer::array<VkExtensionProperties>{}.transient();
    for (std::string_view name : immer::array<std::string_view>{
             "VK_TEST_platform",
             VK_KHR_SURFACE_EXTENSION_NAME,
             VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
             "VK_EXT_surface_maintenance1"})
    {
        VkExtensionProperties property{};
        std::ranges::copy(name, property.extensionName);
        properties.push_back(property);
    }
    return std::move(properties).persistent();
}
}  // namespace

TEST_CASE("[impure] presentation instance configuration owns its window and is reusable and lazy")
{
    backend::MockApi api;
    sdl::detail::TestRuntime runtime{api};
    using environment_type = sdl::detail::TestState<backend::MockApi>;
    char window_storage{};
    SDL_Window * handle = reinterpret_cast<SDL_Window *>(&window_storage);
    std::atomic<bool> is_window_destroyed{false};
    sdl::window_ptr window{handle, [&](auto) { is_window_destroyed.store(true); }};
    immer::array<VkExtensionProperties> properties = presentation_extension_properties();
    immer::array<extension_name> extension_names;
    {
        // The original ReaderIO disappears before its materialized IO executes.
        auto deferred =
            reader_io_discover_presentation_instance_configuration<environment_type>(window)
                .build(runtime.environment())
                .io();
        auto plan = fnfelt::monad::reader_io::lift<environment_type>(std::move(deferred));
        window.reset();
        CHECK_FALSE(is_window_destroyed.load());
        REQUIRE_CALL(api, sdl_vulkan_get_instance_extensions(handle, trompeloeil::_, nullptr))
            .TIMES(2)
            .SIDE_EFFECT(*_2 = 2)
            .SIDE_EFFECT(CHECK(execution::is_main_thread()))
            .LR_SIDE_EFFECT(CHECK_FALSE(is_window_destroyed.load()))
            .RETURN(SDL_TRUE);
        REQUIRE_CALL(
            api, sdl_vulkan_get_instance_extensions(handle, trompeloeil::_, trompeloeil::_))
            .WITH(_3 != nullptr)
            .TIMES(2)
            .SIDE_EFFECT(_3[0] = "VK_TEST_platform")
            .SIDE_EFFECT(_3[1] = VK_KHR_SURFACE_EXTENSION_NAME)
            .RETURN(SDL_TRUE);
        REQUIRE_CALL(
            api, vk_enumerate_instance_extension_properties(nullptr, trompeloeil::_, nullptr))
            .TIMES(2)
            .LR_SIDE_EFFECT(*_2 = static_cast<std::uint32_t>(properties.size()))
            .RETURN(VK_SUCCESS);
        REQUIRE_CALL(
            api,
            vk_enumerate_instance_extension_properties(nullptr, trompeloeil::_, trompeloeil::_))
            .WITH(_3 != nullptr)
            .TIMES(2)
            .LR_SIDE_EFFECT(std::ranges::copy(properties, _3))
            .RETURN(VK_SUCCESS);

        renderer_result<immer::array<extension_name>> first = runtime.run(plan);
        renderer_result<immer::array<extension_name>> second = runtime.run(plan);

        REQUIRE(fnfelt::result::is_success(first));
        REQUIRE(fnfelt::result::is_success(second));
        extension_names = fnfelt::result::get_success(std::move(second));
        CHECK_FALSE(is_window_destroyed.load());
    }
    // Selected names outlive the query window and its reusable plan.
    CHECK(is_window_destroyed.load());
    REQUIRE_EQ(extension_names.size(), 4);
    CHECK_EQ(value_of(extension_names[0]).view(), "VK_TEST_platform");
    CHECK_EQ(value_of(extension_names[3]).view(), "VK_EXT_surface_maintenance1");
}

TEST_CASE(
    "[impure] presentation instance configuration preserves platform and Vulkan query failures")
{
    backend::MockApi api;
    sdl::detail::TestRuntime runtime{api};
    using environment_type = sdl::detail::TestState<backend::MockApi>;
    char window_storage{};
    sdl::window_ptr window{reinterpret_cast<SDL_Window *>(&window_storage), [](auto) {}};
    bool has_sdl_error = true;
    bool has_vulkan_error = false;
    SUBCASE("platform failure") {}
    SUBCASE("Vulkan failure")
    {
        has_sdl_error = false;
        has_vulkan_error = true;
    }
    SUBCASE("both fail and the platform error wins")
    {
        has_vulkan_error = true;
    }
    REQUIRE_CALL(api, sdl_vulkan_get_instance_extensions(window.get(), trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_2 = 0)
        .RETURN(has_sdl_error ? SDL_FALSE : SDL_TRUE);
    std::unique_ptr<trompeloeil::expectation> error_text;
    if (has_sdl_error)
    {
        error_text = NAMED_REQUIRE_CALL(api, sdl_get_error()).RETURN("platform extension failure");
    }
    REQUIRE_CALL(api, vk_enumerate_instance_extension_properties(nullptr, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_2 = 0)
        .RETURN(has_vulkan_error ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS);

    renderer_result<immer::array<extension_name>> outcome = runtime.run(
        reader_io_discover_presentation_instance_configuration<environment_type>(window));

    REQUIRE(fnfelt::result::is_error(outcome));
    if (has_sdl_error)
    {
        sdl::SdlFailure failure = std::get<sdl::SdlFailure>(fnfelt::result::get_error(outcome));
        CHECK_EQ(failure.operation, sdl::FallibleSdlOperation::enumerate_instance_extensions);
        std::string_view message{failure.message.data(), failure.message.size()};
        CHECK_EQ(message, "platform extension failure");
    }
    else
    {
        VulkanFailure failure =
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(outcome)).cause;
        CHECK_EQ(failure.operation, FallibleVulkanOperation::enumerate_instance_extensions);
        CHECK_EQ(failure.result, VK_ERROR_OUT_OF_HOST_MEMORY);
    }
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
