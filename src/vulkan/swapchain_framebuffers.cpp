// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain_framebuffers.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>

#include "../backend/mock_api.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "../sdl/managed_handles_types.hpp"
#include "./errors.hpp"
#include "./managed_handles_types.hpp"
#include "./presentation_device_types.hpp"
#include "./presentation_framebuffers_types.hpp"
#include "./queue_types.hpp"
#include "./renderer_errors.hpp"
#include "./swapchain_framebuffers_types.hpp"
#include "./swapchain_support_types.hpp"

namespace vulkandemo::vulkan
{
namespace
{
auto framebuffer_support() -> SwapchainSupport
{
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.minImageCount = 1;
    capabilities.maxImageCount = 4;
    capabilities.currentExtent = {64, 48};
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

auto framebuffer_image_limits() -> VkImageFormatProperties
{
    return VkImageFormatProperties{{4096, 4096, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT, 1048576};
}
}  // namespace

TEST_CASE("[impure] swapchain framebuffer planning retains all inputs without touching APIs")
{
    backend::MockApi api;
    sdl::detail::TestRuntime runtime{api};
    using environment_type = sdl::detail::TestState<backend::MockApi>;
    char device_storage{}, physical_storage{}, surface_storage{}, window_storage{},
        old_swapchain_storage{}, render_pass_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    surface_ptr surface{reinterpret_cast<VkSurfaceKHR>(&surface_storage), [](auto) {}};
    sdl::window_ptr window{reinterpret_cast<SDL_Window *>(&window_storage), [](auto) {}};
    swapchain_ptr old_swapchain{
        reinterpret_cast<VkSwapchainKHR>(&old_swapchain_storage), [](auto) {}};
    render_pass_ptr render_pass{reinterpret_cast<VkRenderPass>(&render_pass_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    std::weak_ptr surface_observer{surface};
    std::weak_ptr window_observer{window};
    std::weak_ptr old_swapchain_observer{old_swapchain};
    std::weak_ptr render_pass_observer{render_pass};

    {
        auto plan = reader_io_create_swapchain_framebuffers<environment_type>(
            PresentationDevice{
                std::move(surface),
                std::move(device),
                reinterpret_cast<VkPhysicalDevice>(&physical_storage),
                {},
                {},
                {}},
            std::move(window),
            std::move(old_swapchain),
            CompatibleRenderPass{VK_FORMAT_B8G8R8A8_UNORM, std::move(render_pass)});
        [[maybe_unused]] auto discarded = plan.build(runtime.environment());

        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(surface_observer.expired());
        CHECK_FALSE(window_observer.expired());
        CHECK_FALSE(old_swapchain_observer.expired());
        CHECK_FALSE(render_pass_observer.expired());
    }

    CHECK(device_observer.expired());
    CHECK(surface_observer.expired());
    CHECK(window_observer.expired());
    CHECK(old_swapchain_observer.expired());
    CHECK(render_pass_observer.expired());
}

TEST_CASE(
    "[impure] swapchain framebuffer assembly builds initial framebuffers, reuses compatible "
    "passes, and cleans up failures")
{
    backend::MockApi api;
    sdl::detail::TestRuntime runtime{api};
    using environment_type = sdl::detail::TestState<backend::MockApi>;
    char device_storage{}, physical_storage{}, surface_storage{}, window_storage{};
    char old_swapchain_storage{}, new_swapchain_storage{}, image_storage{}, view_storage{};
    char old_pass_storage{}, new_pass_storage{}, framebuffer_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkPhysicalDevice physical = reinterpret_cast<VkPhysicalDevice>(&physical_storage);
    VkSurfaceKHR surface_handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    SDL_Window * window_handle = reinterpret_cast<SDL_Window *>(&window_storage);
    VkSwapchainKHR old_swapchain_handle = reinterpret_cast<VkSwapchainKHR>(&old_swapchain_storage);
    VkSwapchainKHR new_swapchain_handle = reinterpret_cast<VkSwapchainKHR>(&new_swapchain_storage);
    VkImage image_handle = reinterpret_cast<VkImage>(&image_storage);
    VkImageView view_handle = reinterpret_cast<VkImageView>(&view_storage);
    VkRenderPass old_pass_handle = reinterpret_cast<VkRenderPass>(&old_pass_storage);
    VkRenderPass new_pass_handle = reinterpret_cast<VkRenderPass>(&new_pass_storage);
    VkFramebuffer framebuffer_handle = reinterpret_cast<VkFramebuffer>(&framebuffer_storage);

    PresentationDevice presentation_device{
        surface_ptr{surface_handle, [](auto) {}},
        device_ptr{device_handle, [](auto) {}},
        physical,
        {queue_family_index{0}, queue_family_index{0}},
        {},
        {}};
    sdl::window_ptr window{window_handle, [](auto) {}};

    bool has_old_swapchain = false;
    bool has_existing_pass = false;
    VkFormat existing_pass_format = VK_FORMAT_B8G8R8A8_UNORM;
    VkResult render_pass_status = VK_SUCCESS;
    VkResult framebuffer_status = VK_SUCCESS;
    SUBCASE("initial assembly creates swapchain, pass and framebuffers") {}
    SUBCASE("replacement with matching format reuses render pass with zero render-pass calls")
    {
        has_old_swapchain = true;
        has_existing_pass = true;
    }
    SUBCASE("replacement with changed format creates a new render pass")
    {
        has_old_swapchain = true;
        has_existing_pass = true;
        existing_pass_format = VK_FORMAT_R8G8B8A8_UNORM;
    }
    SUBCASE("render-pass failure cleans up newly acquired swapchain and views")
    {
        render_pass_status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    SUBCASE("framebuffer failure cleans up newly acquired views and swapchain")
    {
        framebuffer_status = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    bool const should_create_pass =
        !has_existing_pass || existing_pass_format != VK_FORMAT_B8G8R8A8_UNORM;
    bool const should_succeed =
        render_pass_status == VK_SUCCESS && framebuffer_status == VK_SUCCESS;

    std::atomic<bool> is_old_swapchain_destroyed{false};
    std::atomic<bool> is_old_pass_destroyed{false};
    swapchain_ptr old_swapchain;
    std::weak_ptr<void> old_swapchain_observer;
    if (has_old_swapchain)
    {
        old_swapchain = {
            old_swapchain_handle, [&](auto) { is_old_swapchain_destroyed.store(true); }};
        old_swapchain_observer = old_swapchain;
    }
    render_pass_ptr existing_pass;
    if (has_existing_pass)
    {
        existing_pass = {old_pass_handle, [&](auto) { is_old_pass_destroyed.store(true); }};
    }
    CompatibleRenderPass compatible_pass{
        has_existing_pass ? existing_pass_format : VK_FORMAT_UNDEFINED, std::move(existing_pass)};

    SwapchainSupport support = framebuffer_support();
    REQUIRE_CALL(api, sdl_vulkan_get_drawable_size(window_handle, trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(*_2 = 64)
        .SIDE_EFFECT(*_3 = 48);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_capabilities_khr(physical, surface_handle, trompeloeil::_))
        .LR_SIDE_EFFECT(*_3 = support.capabilities)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical, surface_handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical, surface_handle, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr)
        .LR_SIDE_EFFECT(*_4 = support.formats[0])
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_present_modes_khr(
            physical, surface_handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_present_modes_khr(
            physical, surface_handle, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr)
        .LR_SIDE_EFFECT(*_4 = support.present_modes[0])
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_image_format_properties(
            physical,
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            0,
            trompeloeil::_))
        .SIDE_EFFECT(*_7 = framebuffer_image_limits())
        .RETURN(VK_SUCCESS);

    VkSwapchainKHR const expected_old = has_old_swapchain ? old_swapchain_handle : VK_NULL_HANDLE;
    REQUIRE_CALL(
        api, vk_create_swapchain_khr(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_WITH(
            _2->oldSwapchain == expected_old &&
            (!has_old_swapchain || !is_old_swapchain_destroyed.load()))
        .LR_SIDE_EFFECT(*_4 = new_swapchain_handle)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_get_swapchain_images_khr(device_handle, new_swapchain_handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_get_swapchain_images_khr(
            device_handle, new_swapchain_handle, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr)
        .SIDE_EFFECT(*_4 = image_handle)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_create_image_view(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
        .WITH(_2->image == image_handle && _2->format == VK_FORMAT_B8G8R8A8_UNORM)
        .LR_SIDE_EFFECT(*_4 = view_handle)
        .RETURN(VK_SUCCESS);

    std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
    if (should_create_pass)
    {
        expectations.push_back(
            NAMED_REQUIRE_CALL(
                api, vk_create_render_pass(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
                .WITH(_2->pAttachments[0].format == VK_FORMAT_B8G8R8A8_UNORM)
                .LR_SIDE_EFFECT(*_4 = new_pass_handle)
                .LR_RETURN(render_pass_status));
        if (render_pass_status == VK_SUCCESS)
        {
            expectations.push_back(NAMED_REQUIRE_CALL(
                api, vk_destroy_render_pass(device_handle, new_pass_handle, nullptr)));
        }
    }
    if (render_pass_status == VK_SUCCESS)
    {
        VkRenderPass const expected_pass = should_create_pass ? new_pass_handle : old_pass_handle;
        expectations.push_back(
            NAMED_REQUIRE_CALL(api, vk_get_physical_device_properties(physical, trompeloeil::_))
                .SIDE_EFFECT(_2->limits.maxFramebufferWidth = 4096)
                .SIDE_EFFECT(_2->limits.maxFramebufferHeight = 4096));
        expectations.push_back(
            NAMED_REQUIRE_CALL(
                api, vk_create_framebuffer(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
                .WITH(_2->renderPass == expected_pass && _2->pAttachments[0] == view_handle)
                .LR_SIDE_EFFECT(*_4 = framebuffer_handle)
                .LR_RETURN(framebuffer_status));
        if (framebuffer_status == VK_SUCCESS)
        {
            expectations.push_back(NAMED_REQUIRE_CALL(
                api, vk_destroy_framebuffer(device_handle, framebuffer_handle, nullptr)));
        }
    }
    expectations.push_back(
        NAMED_REQUIRE_CALL(api, vk_destroy_image_view(device_handle, view_handle, nullptr)));
    expectations.push_back(NAMED_REQUIRE_CALL(
        api, vk_destroy_swapchain_khr(device_handle, new_swapchain_handle, nullptr)));

    presentation_framebuffers_result outcome = runtime.run(
        reader_io_create_swapchain_framebuffers<environment_type>(
            std::move(presentation_device),
            std::move(window),
            std::move(old_swapchain),
            std::move(compatible_pass)));

    if (has_old_swapchain)
    {
        CHECK(is_old_swapchain_destroyed.load());
        CHECK(old_swapchain_observer.expired());
    }
    if (has_existing_pass && existing_pass_format != VK_FORMAT_B8G8R8A8_UNORM)
    {
        CHECK(is_old_pass_destroyed.load());
    }
    if (should_succeed)
    {
        REQUIRE(fnfelt::result::is_success(outcome));
        PresentationFramebuffers const & assembled = fnfelt::result::get_success(outcome);
        CHECK_EQ(
            assembled.presentation_image_views.presentation_swapchain_images.presentation_swapchain
                .swapchain.get(),
            new_swapchain_handle);
        CHECK_EQ(
            assembled.render_pass.get(), should_create_pass ? new_pass_handle : old_pass_handle);
        REQUIRE_EQ(assembled.framebuffers.size(), 1U);
        CHECK_EQ(assembled.framebuffers[0].get(), framebuffer_handle);
    }
    else
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        VulkanFailure failure =
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(outcome)).cause;
        if (render_pass_status != VK_SUCCESS)
        {
            CHECK_EQ(failure.operation, FallibleVulkanOperation::create_render_pass);
            CHECK_EQ(failure.result, render_pass_status);
        }
        else
        {
            CHECK_EQ(failure.operation, FallibleVulkanOperation::create_framebuffer);
            CHECK_EQ(failure.result, framebuffer_status);
        }
    }
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
