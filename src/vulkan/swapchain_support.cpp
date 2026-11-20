// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain_support.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <ranges>
#include <utility>

#include <SDL2/SDL_video.h>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>
#include <immer/box.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "../sdl/instance_extensions.hpp"
#include "../sdl/video.hpp"
#include "../sdl/vulkan_surface.hpp"
#include "../sdl/window.hpp"
#include "../sdl/window_types.hpp"
#include "../text/string.hpp"
#include "./instance.hpp"
#include "./physical_device_selection.hpp"
#include "./physical_device_selection_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct SwapchainSupportEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};

struct SwapchainSupportFixture
{
    backend::MockApi api;
    char instance_storage{}, device_storage{}, surface_storage{};
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&device_storage);
    VkSurfaceKHR surface_handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    instance_ptr instance{reinterpret_cast<VkInstance>(&instance_storage), [](auto) {}};
    surface_ptr surface{surface_handle, [parent = instance](auto) mutable { parent.reset(); }};
    immer::array_transient<std::shared_ptr<trompeloeil::expectation>> expectations;
    VkSurfaceCapabilitiesKHR capabilities{};
    immer::array<VkSurfaceFormatKHR> formats{
        VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    immer::array<VkPresentModeKHR> present_modes{VK_PRESENT_MODE_FIFO_KHR};

    SwapchainSupportFixture()
    {
        capabilities.minImageCount = 2;
        capabilities.maxImageCount = 4;
    }

    void keep(std::unique_ptr<trompeloeil::expectation> expectation)
    {
        expectations.push_back(std::shared_ptr<trompeloeil::expectation>{std::move(expectation)});
    }

    template <std::size_t runs = 1>
    void expect_queries(
        VkResult capability_status = VK_SUCCESS,
        VkResult format_status = VK_SUCCESS,
        VkResult present_mode_status = VK_SUCCESS)
    {
        keep(NAMED_REQUIRE_CALL(
                 api,
                 vk_get_physical_device_surface_capabilities_khr(
                     physical_device, surface_handle, trompeloeil::_))
                 .WITH(_3 != nullptr)
                 .SIDE_EFFECT(*_3 = capabilities)
                 .RETURN(capability_status)
                 .TIMES(runs));
        keep(NAMED_REQUIRE_CALL(
                 api,
                 vk_get_physical_device_surface_formats_khr(
                     physical_device, surface_handle, trompeloeil::_, nullptr))
                 .SIDE_EFFECT(*_3 = static_cast<std::uint32_t>(formats.size()))
                 .RETURN(format_status)
                 .TIMES(runs));
        if (format_status == VK_SUCCESS && !formats.empty())
        {
            keep(NAMED_REQUIRE_CALL(
                     api,
                     vk_get_physical_device_surface_formats_khr(
                         physical_device, surface_handle, trompeloeil::_, trompeloeil::_))
                     .WITH(_4 != nullptr && *_3 == formats.size())
                     .SIDE_EFFECT(std::ranges::copy(formats, _4))
                     .RETURN(VK_SUCCESS)
                     .TIMES(runs));
        }
        keep(NAMED_REQUIRE_CALL(
                 api,
                 vk_get_physical_device_surface_present_modes_khr(
                     physical_device, surface_handle, trompeloeil::_, nullptr))
                 .SIDE_EFFECT(*_3 = static_cast<std::uint32_t>(present_modes.size()))
                 .RETURN(present_mode_status)
                 .TIMES(runs));
        if (present_mode_status == VK_SUCCESS && !present_modes.empty())
        {
            keep(NAMED_REQUIRE_CALL(
                     api,
                     vk_get_physical_device_surface_present_modes_khr(
                         physical_device, surface_handle, trompeloeil::_, trompeloeil::_))
                     .WITH(_4 != nullptr && *_3 == present_modes.size())
                     .SIDE_EFFECT(std::ranges::copy(present_modes, _4))
                     .RETURN(VK_SUCCESS)
                     .TIMES(runs));
        }
    }
};
}  // namespace

TEST_CASE("[impure] swapchain support plans retain parents without querying")
{
    SwapchainSupportFixture fixture;
    SwapchainSupportEnvironment environment{std::ref(fixture.api)};
    std::weak_ptr<surface_ptr::element_type> surface_observer = fixture.surface;
    std::weak_ptr<instance_ptr::element_type> instance_observer = fixture.instance;

    {
        // Constructing or binding the composition retains owners without executing its queries.
        auto plan = reader_io_query_swapchain_support<decltype(environment)>(
            fixture.physical_device, std::move(fixture.surface));
        [[maybe_unused]] auto deferred = plan.build(environment);
        fixture.instance.reset();

        CHECK_FALSE(surface_observer.expired());
        CHECK_FALSE(instance_observer.expired());
    }

    CHECK(surface_observer.expired());
    CHECK(instance_observer.expired());
}

TEST_CASE("[impure] swapchain support collects reusable owned snapshots including empty lists")
{
    SwapchainSupportFixture fixture;
    SwapchainSupportEnvironment environment{std::ref(fixture.api)};
    SUBCASE("available support") {}
    SUBCASE("empty formats")
    {
        fixture.formats = {};
    }
    SUBCASE("empty present modes")
    {
        fixture.present_modes = {};
    }
    SUBCASE("empty query snapshots")
    {
        fixture.formats = {};
        fixture.present_modes = {};
    }
    fixture.expect_queries<2>();
    auto plan = reader_io_query_swapchain_support<decltype(environment)>(
        fixture.physical_device, std::move(fixture.surface));

    // Both runs complete all independent queries and produce values independent of the plans.
    swapchain_support_result first = plan.build(environment).sync_wait(lf::lazy_pool{2});
    swapchain_support_result second =
        std::move(plan).build(environment).sync_wait(lf::lazy_pool{2});

    for (auto const * outcome : {&first, &second})
    {
        REQUIRE(fnfelt::result::is_success(*outcome));
        SwapchainSupport const & support = fnfelt::result::get_success(*outcome);
        CHECK_EQ(support.capabilities.minImageCount, 2U);
        CHECK_EQ(support.capabilities.maxImageCount, 4U);
        REQUIRE_EQ(support.formats.size(), fixture.formats.size());
        REQUIRE_EQ(support.present_modes.size(), fixture.present_modes.size());
        if (!support.formats.empty())
        {
            CHECK_EQ(support.formats[0].format, fixture.formats[0].format);
            CHECK_EQ(support.formats[0].colorSpace, fixture.formats[0].colorSpace);
        }
        if (!support.present_modes.empty())
        {
            CHECK_EQ(support.present_modes[0], fixture.present_modes[0]);
        }
    }
}

TEST_CASE("[impure] swapchain support completes every query before reporting failures")
{
    SwapchainSupportFixture fixture;
    SwapchainSupportEnvironment environment{std::ref(fixture.api)};
    VkResult capability_status = VK_SUCCESS;
    VkResult format_status = VK_SUCCESS;
    VkResult present_mode_status = VK_SUCCESS;
    FallibleVulkanOperation expected =
        FallibleVulkanOperation::get_physical_device_surface_capabilities;
    VkResult expected_status = VK_ERROR_SURFACE_LOST_KHR;
    SUBCASE("capability error precedes other failures")
    {
        capability_status = VK_ERROR_SURFACE_LOST_KHR;
        format_status = VK_ERROR_OUT_OF_HOST_MEMORY;
        present_mode_status = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    SUBCASE("format error precedes present-mode failure")
    {
        format_status = VK_ERROR_SURFACE_LOST_KHR;
        present_mode_status = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        expected = FallibleVulkanOperation::get_physical_device_surface_formats;
    }
    SUBCASE("present-mode error")
    {
        present_mode_status = VK_ERROR_SURFACE_LOST_KHR;
        expected = FallibleVulkanOperation::get_physical_device_surface_present_modes;
    }
    fixture.expect_queries(capability_status, format_status, present_mode_status);

    // All installed expectations must be satisfied even if a different query reports an error.
    swapchain_support_result outcome = reader_io_query_swapchain_support<decltype(environment)>(
                                           fixture.physical_device, std::move(fixture.surface))
                                           .build(environment)
                                           .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK(fnfelt::result::get_error(outcome).operation == expected);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, expected_status);
}

TEST_CASE("[integration][windowed] a real presentation surface exposes swapchain support")
{
    backend::Api api;
    using environment_type = sdl::detail::TestState<backend::Api>;
    using state_type = immer::box<environment_type>;
    sdl::detail::TestRuntime runtime{api};
    REQUIRE(
        fnfelt::result::is_success(runtime.run(sdl::state_io_sdl_initialize_video<state_type>())));
    sdl::WindowOptions options{
        .width = sdl::window_width{64},
        .height = sdl::window_height{48},
        .flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)};
    sdl::sdl_result<SDL_Window *> window =
        runtime.run(sdl::state_io_sdl_create_window<state_type>(options));
    REQUIRE(fnfelt::result::is_success(window));
    sdl::sdl_result<immer::array<text::String>> extensions = runtime.run(
        sdl::reader_io_sdl_vulkan_get_instance_extensions<environment_type>(
            fnfelt::result::get_success(window)));
    REQUIRE(fnfelt::result::is_success(extensions));
    vulkan_result<instance_ptr> instance = runtime.run(
        reader_io_vk_create_instance<environment_type>(fnfelt::result::get_success(extensions)));
    REQUIRE(fnfelt::result::is_success(instance));
    sdl::sdl_result<surface_ptr> surface = runtime.run(
        sdl::reader_io_sdl_vulkan_create_surface<environment_type>(
            runtime.resources().window_owner, fnfelt::result::get_success(instance)));
    REQUIRE(fnfelt::result::is_success(surface));
    presentation_physical_device_selection_result selected = runtime.run(
        reader_io_select_presentation_physical_device<environment_type>(
            fnfelt::result::get_success(instance), fnfelt::result::get_success(surface)));
    REQUIRE(fnfelt::result::is_success(selected));

    // The selected device already supports this surface; no logical device is needed to query it.
    swapchain_support_result outcome = runtime.run(
        reader_io_query_swapchain_support<environment_type>(
            fnfelt::result::get_success(selected).physical_device,
            fnfelt::result::get_success(selected).surface));

    REQUIRE(fnfelt::result::is_success(outcome));
    SwapchainSupport const & support = fnfelt::result::get_success(outcome);
    CHECK_GE(support.capabilities.minImageCount, 1U);
    CHECK_FALSE(support.formats.empty());
    CHECK(
        std::ranges::find(support.present_modes, VK_PRESENT_MODE_FIFO_KHR) !=
        support.present_modes.end());
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
