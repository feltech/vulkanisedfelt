// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_swapchain.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <variant>

#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/box.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "../sdl/instance_extensions.hpp"
#include "../sdl/video.hpp"
#include "../sdl/vulkan_surface.hpp"
#include "../sdl/window.hpp"
#include "./extension_types.hpp"
#include "./instance.hpp"
#include "./presentation_device.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
auto drawable_support() -> SwapchainSupport
{
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.minImageCount = 1;
    capabilities.maxImageCount = 4;
    capabilities.currentExtent = {
        std::numeric_limits<std::uint32_t>::max(), std::numeric_limits<std::uint32_t>::max()};
    capabilities.minImageExtent = {1, 1};
    capabilities.maxImageExtent = {4096, 4096};
    capabilities.maxImageArrayLayers = 1;
    capabilities.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    capabilities.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    capabilities.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    return SwapchainSupport{
        capabilities,
        {{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}},
        {VK_PRESENT_MODE_FIFO_KHR}};
}

auto image_limits() -> VkImageFormatProperties
{
    return VkImageFormatProperties{{4096, 4096, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT, 1048576};
}
}  // namespace

TEST_CASE("[impure] presentation swapchain plans retain inputs without touching APIs")
{
    backend::MockApi api;
    sdl::detail::TestRuntime runtime{api};
    using environment_type = sdl::detail::TestState<backend::MockApi>;
    char device_storage{}, physical_storage{}, surface_storage{}, window_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    surface_ptr surface{reinterpret_cast<VkSurfaceKHR>(&surface_storage), [](auto) {}};
    sdl::window_ptr window{reinterpret_cast<SDL_Window *>(&window_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    std::weak_ptr surface_observer{surface};
    std::weak_ptr window_observer{window};

    {
        auto plan = reader_io_create_presentation_swapchain<environment_type>(
            PresentationDevice{
                std::move(surface),
                std::move(device),
                reinterpret_cast<VkPhysicalDevice>(&physical_storage),
                {},
                {},
                {}},
            std::move(window));
        [[maybe_unused]] auto discarded = plan.build(runtime.environment());

        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(surface_observer.expired());
        CHECK_FALSE(window_observer.expired());
    }

    CHECK(device_observer.expired());
    CHECK(surface_observer.expired());
    CHECK(window_observer.expired());
}

TEST_CASE(
    "[impure] presentation swapchain setup validates live pixels and bypasses later effects on "
    "failure")
{
    backend::MockApi api;
    sdl::detail::TestRuntime runtime{api};
    using environment_type = sdl::detail::TestState<backend::MockApi>;
    std::thread::id main_thread = std::this_thread::get_id();
    char device_storage{}, physical_storage{}, surface_storage{}, window_storage{},
        swapchain_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkPhysicalDevice physical = reinterpret_cast<VkPhysicalDevice>(&physical_storage);
    VkSurfaceKHR surface_handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    SDL_Window * window_handle = reinterpret_cast<SDL_Window *>(&window_storage);
    VkSwapchainKHR swapchain_handle = reinterpret_cast<VkSwapchainKHR>(&swapchain_storage);
    PresentationDevice presentation_device{
        surface_ptr{surface_handle, [](auto) {}},
        device_ptr{device_handle, [](auto) {}},
        physical,
        {queue_family_index{1}, queue_family_index{1}},
        {},
        {}};
    sdl::window_ptr window{window_handle, [](auto) {}};
    SwapchainSupport support = drawable_support();
    VkImageFormatProperties limits = image_limits();
    VkResult support_status = VK_SUCCESS;
    VkResult image_status = VK_SUCCESS;
    VkResult create_status = VK_SUCCESS;
    int width = 800;
    bool should_query_image = true;
    bool should_create = true;
    bool is_domain_failure = false;
    constexpr int runs = 2;
    SwapchainConfigurationFailure reason =
        SwapchainConfigurationFailure::no_acceptable_surface_format;
    SUBCASE("success and replay after final release") {}
    SUBCASE("support failure")
    {
        support_status = VK_ERROR_SURFACE_LOST_KHR;
        should_query_image = false;
        should_create = false;
    }
    SUBCASE("zero drawable")
    {
        width = 0;
        should_query_image = false;
        should_create = false;
        is_domain_failure = true;
        reason = SwapchainConfigurationFailure::unavailable_extent;
    }
    SUBCASE("missing acceptable format")
    {
        support.formats = {};
        should_query_image = false;
        should_create = false;
        is_domain_failure = true;
    }
    SUBCASE("missing preferred mode")
    {
        support.present_modes = {};
        should_query_image = false;
        should_create = false;
        is_domain_failure = true;
        reason = SwapchainConfigurationFailure::no_preferred_present_mode;
    }
    SUBCASE("unsupported image format")
    {
        image_status = VK_ERROR_FORMAT_NOT_SUPPORTED;
        should_create = false;
        is_domain_failure = true;
        reason = SwapchainConfigurationFailure::unsupported_image_parameters;
    }
    SUBCASE("image query allocation failure")
    {
        image_status = VK_ERROR_OUT_OF_HOST_MEMORY;
        should_create = false;
    }
    SUBCASE("image size exceeds physical limit")
    {
        limits.maxExtent.width = 799;
        should_create = false;
        is_domain_failure = true;
        reason = SwapchainConfigurationFailure::unsupported_image_parameters;
    }
    SUBCASE("surface changes before create")
    {
        create_status = VK_ERROR_OUT_OF_DATE_KHR;
    }
    REQUIRE_CALL(api, sdl_vulkan_get_drawable_size(window_handle, trompeloeil::_, trompeloeil::_))
        .WITH(std::this_thread::get_id() == main_thread)
        .LR_SIDE_EFFECT(*_2 = width)
        .SIDE_EFFECT(*_3 = 600)
        .TIMES(runs);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_capabilities_khr(physical, surface_handle, trompeloeil::_))
        .LR_SIDE_EFFECT(*_3 = support.capabilities)
        .LR_RETURN(support_status)
        .TIMES(runs);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical, surface_handle, trompeloeil::_, nullptr))
        .LR_SIDE_EFFECT(*_3 = static_cast<std::uint32_t>(support.formats.size()))
        .RETURN(VK_SUCCESS)
        .TIMES(runs);
    std::unique_ptr<trompeloeil::expectation> format_fill;
    if (!support.formats.empty())
    {
        format_fill = NAMED_REQUIRE_CALL(
                          api,
                          vk_get_physical_device_surface_formats_khr(
                              physical, surface_handle, trompeloeil::_, trompeloeil::_))
                          .WITH(_4 != nullptr)
                          .LR_SIDE_EFFECT(*_4 = support.formats[0])
                          .RETURN(VK_SUCCESS)
                          .TIMES(runs);
    }
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_present_modes_khr(
            physical, surface_handle, trompeloeil::_, nullptr))
        .LR_SIDE_EFFECT(*_3 = static_cast<std::uint32_t>(support.present_modes.size()))
        .RETURN(VK_SUCCESS)
        .TIMES(runs);
    std::unique_ptr<trompeloeil::expectation> mode_fill;
    if (!support.present_modes.empty())
    {
        mode_fill = NAMED_REQUIRE_CALL(
                        api,
                        vk_get_physical_device_surface_present_modes_khr(
                            physical, surface_handle, trompeloeil::_, trompeloeil::_))
                        .WITH(_4 != nullptr)
                        .LR_SIDE_EFFECT(*_4 = support.present_modes[0])
                        .RETURN(VK_SUCCESS)
                        .TIMES(runs);
    }
    std::unique_ptr<trompeloeil::expectation> image_query;
    if (should_query_image)
    {
        image_query = NAMED_REQUIRE_CALL(
                          api,
                          vk_get_physical_device_image_format_properties(
                              physical,
                              VK_FORMAT_B8G8R8A8_UNORM,
                              VK_IMAGE_TYPE_2D,
                              VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                              0,
                              trompeloeil::_))
                          .LR_SIDE_EFFECT(*_7 = limits)
                          .LR_RETURN(image_status)
                          .TIMES(runs);
    }
    std::unique_ptr<trompeloeil::expectation> create;
    std::unique_ptr<trompeloeil::expectation> destroy;
    if (should_create)
    {
        create =
            NAMED_REQUIRE_CALL(
                api,
                vk_create_swapchain_khr(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
                .LR_SIDE_EFFECT(CHECK_EQ(_2->imageExtent.width, 800U))
                .LR_SIDE_EFFECT(CHECK_EQ(_2->imageExtent.height, 600U))
                .SIDE_EFFECT(*_4 = swapchain_handle)
                .LR_RETURN(create_status)
                .TIMES(runs);
        if (create_status == VK_SUCCESS)
        {
            destroy = NAMED_REQUIRE_CALL(
                          api, vk_destroy_swapchain_khr(device_handle, swapchain_handle, nullptr))
                          .TIMES(runs);
        }
    }

    std::weak_ptr device_observer{presentation_device.device};
    std::weak_ptr surface_observer{presentation_device.surface};

    presentation_swapchain_result outcome = [&]
    {
        auto plan =
            reader_io_create_presentation_swapchain<environment_type>(presentation_device, window);
        if (runs == 2)
        {
            // The first successful owner is released before this initial-only plan is replayed.
            presentation_swapchain_result first = runtime.run(plan);
            CHECK_EQ(
                fnfelt::result::is_success(first), should_create && create_status == VK_SUCCESS);
        }
        return runtime.run(plan);
    }();
    presentation_device.device.reset();
    presentation_device.surface.reset();

    if (is_domain_failure)
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        CHECK(
            std::get<NoSuitableSwapchainConfiguration>(fnfelt::result::get_error(outcome)).reason ==
            reason);
    }
    else if (
        support_status != VK_SUCCESS || image_status != VK_SUCCESS || create_status != VK_SUCCESS)
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        VulkanFailure failure =
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(outcome)).cause;
        CHECK_EQ(
            failure.result,
            support_status != VK_SUCCESS     ? support_status
                : image_status != VK_SUCCESS ? image_status
                                             : create_status);
    }
    else
    {
        REQUIRE(fnfelt::result::is_success(outcome));
        PresentationSwapchain const & created = fnfelt::result::get_success(outcome);
        CHECK_EQ(created.swapchain.get(), swapchain_handle);
        CHECK_EQ(created.presentation_device.device.get(), device_handle);
        CHECK_EQ(created.presentation_device.surface.get(), surface_handle);
        CHECK_EQ(created.configuration.extent.width, 800U);
        CHECK(created.configuration.image_count == swapchain_image_count{2});
        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(surface_observer.expired());
    }
    CHECK_FALSE(runtime.resources().window_owner);
}

TEST_CASE(
    "[integration][windowed] an SDL presentation device creates and releases its initial swapchain")
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
    sdl::sdl_result<immer::array<extension_name>> extensions = runtime.run(
        sdl::reader_io_sdl_vulkan_get_instance_extensions<environment_type>(
            fnfelt::result::get_success(window)));
    REQUIRE(fnfelt::result::is_success(extensions));
    renderer_result<instance_ptr> instance = runtime.run(
        reader_io_vk_create_instance<environment_type>(fnfelt::result::get_success(extensions)));
    REQUIRE(fnfelt::result::is_success(instance));
    sdl::sdl_result<surface_ptr> surface = runtime.run(
        sdl::reader_io_sdl_vulkan_create_surface<environment_type>(
            runtime.resources().window_owner, fnfelt::result::get_success(instance)));
    REQUIRE(fnfelt::result::is_success(surface));
    presentation_device_result device = runtime.run(
        reader_io_create_presentation_device<environment_type>(
            fnfelt::result::get_success(instance), fnfelt::result::get_success(surface)));
    REQUIRE(fnfelt::result::is_success(device));

    // No images are acquired and no GPU work is submitted; final release can destroy immediately.
    presentation_swapchain_result outcome = runtime.run(
        reader_io_create_presentation_swapchain<environment_type>(
            fnfelt::result::get_success(device), runtime.resources().window_owner));

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK(fnfelt::result::get_success(outcome).swapchain != nullptr);
    CHECK_GT(fnfelt::result::get_success(outcome).configuration.extent.width, 0U);
    CHECK_GT(fnfelt::result::get_success(outcome).configuration.extent.height, 0U);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
