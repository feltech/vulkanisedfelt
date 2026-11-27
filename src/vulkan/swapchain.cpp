// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/mock_api.hpp"
#include "./errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct SwapchainEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] initial swapchain planning is deferred and retains both parents")
{
    backend::MockApi api;
    SwapchainEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    surface_ptr surface{reinterpret_cast<VkSurfaceKHR>(&surface_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    std::weak_ptr surface_observer{surface};

    {
        auto plan = reader_io_vk_create_swapchain_khr<SwapchainEnvironment>(
            PresentationDevice{std::move(surface), std::move(device), {}, {}, {}, {}}, {}, {});
        [[maybe_unused]] auto discarded = plan.build(environment);

        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(surface_observer.expired());
    }

    CHECK(device_observer.expired());
    CHECK(surface_observer.expired());
}

TEST_CASE(
    "[impure] initial swapchain creation forwards ordinary shared or split queue configuration")
{
    backend::MockApi api;
    SwapchainEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{}, swapchain_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkSurfaceKHR surface_handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    VkSwapchainKHR swapchain_handle = reinterpret_cast<VkSwapchainKHR>(&swapchain_storage);
    bool has_separate_families = false;
    VkCompositeAlphaFlagsKHR supported_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    SUBCASE("shared queues and opaque alpha") {}
    SUBCASE("split queues and fallback alpha")
    {
        has_separate_families = true;
        supported_alpha =
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }
    PresentationDevice presentation_device{
        surface_ptr{surface_handle, [](auto) {}},
        device_ptr{device_handle, [](auto) {}},
        {},
        {queue_family_index{2}, queue_family_index{has_separate_families ? 5U : 2U}},
        {},
        {}};
    SwapchainConfiguration configuration{
        {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        VK_PRESENT_MODE_FIFO_KHR,
        {800, 600},
        swapchain_image_count{3}};
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    capabilities.currentTransform = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    capabilities.supportedCompositeAlpha = supported_alpha;
    REQUIRE_CALL(
        api, vk_create_swapchain_khr(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->sType, VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->pNext, nullptr))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->flags, 0U))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->surface, surface_handle))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->minImageCount, 3U))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->imageFormat, configuration.surface_format.format))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->imageColorSpace, configuration.surface_format.colorSpace))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->imageExtent.width, 800U))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->imageExtent.height, 600U))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->imageArrayLayers, 1U))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->imageUsage, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
        .LR_SIDE_EFFECT(CHECK_EQ(
            _2->imageSharingMode,
            has_separate_families ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->queueFamilyIndexCount, has_separate_families ? 2U : 0U))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->pQueueFamilyIndices == nullptr, !has_separate_families))
        .LR_SIDE_EFFECT(if (has_separate_families) {
            CHECK_EQ(_2->pQueueFamilyIndices[0], 2U);
            CHECK_EQ(_2->pQueueFamilyIndices[1], 5U);
        })
        .LR_SIDE_EFFECT(CHECK_EQ(_2->preTransform, capabilities.currentTransform))
        .LR_SIDE_EFFECT(CHECK_EQ(
            _2->compositeAlpha,
            has_separate_families ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
                                  : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->presentMode, VK_PRESENT_MODE_FIFO_KHR))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->clipped, VK_TRUE))
        .LR_SIDE_EFFECT(CHECK_EQ(_2->oldSwapchain, VK_NULL_HANDLE))
        .SIDE_EFFECT(*_4 = swapchain_handle)
        .RETURN(VK_SUCCESS)
        .TIMES(2);
    REQUIRE_CALL(api, vk_destroy_swapchain_khr(device_handle, swapchain_handle, nullptr)).TIMES(2);
    auto plan = reader_io_vk_create_swapchain_khr<SwapchainEnvironment>(
        presentation_device, configuration, capabilities);

    // Each replay follows final release of the earlier initial swapchain.
    for (int iteration = 0; iteration < 2; ++iteration)
    {
        vulkan_result<swapchain_ptr> outcome = plan.build(environment).sync_wait(lf::lazy_pool{2});

        REQUIRE(fnfelt::result::is_success(outcome));
        CHECK_EQ(fnfelt::result::get_success(outcome).get(), swapchain_handle);
    }
}

TEST_CASE("[impure] initial swapchain creation preserves Vulkan errors without adopting a handle")
{
    backend::MockApi api;
    SwapchainEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{};
    PresentationDevice presentation_device{
        surface_ptr{reinterpret_cast<VkSurfaceKHR>(&surface_storage), [](auto) {}},
        device_ptr{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}},
        {},
        {},
        {},
        {}};
    SwapchainConfiguration configuration{
        {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        VK_PRESENT_MODE_FIFO_KHR,
        {640, 480},
        swapchain_image_count{2}};
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    capabilities.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkResult status = VK_ERROR_OUT_OF_DATE_KHR;
    SUBCASE("surface changed") {}
    SUBCASE("native window already used")
    {
        status = VK_ERROR_NATIVE_WINDOW_IN_USE_KHR;
    }
    SUBCASE("allocation failed")
    {
        status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    REQUIRE_CALL(
        api,
        vk_create_swapchain_khr(
            presentation_device.device.get(), trompeloeil::_, nullptr, trompeloeil::_))
        .LR_RETURN(status);

    vulkan_result<swapchain_ptr> outcome = reader_io_vk_create_swapchain_khr<SwapchainEnvironment>(
                                               presentation_device, configuration, capabilities)
                                               .build(environment)
                                               .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK(
        fnfelt::result::get_error(outcome).operation == FallibleVulkanOperation::create_swapchain);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, status);
}

TEST_CASE("[impure] swapchain replacement retains old swapchain only through creation")
{
    backend::MockApi api;
    SwapchainEnvironment environment{std::ref(api)};
    char device_storage{}, surface_storage{}, old_storage{}, new_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkSurfaceKHR surface_handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    VkSwapchainKHR old_handle = reinterpret_cast<VkSwapchainKHR>(&old_storage);
    VkSwapchainKHR new_handle = reinterpret_cast<VkSwapchainKHR>(&new_storage);
    PresentationDevice presentation_device{
        surface_ptr{surface_handle, [](auto) {}},
        device_ptr{device_handle, [](auto) {}},
        {},
        {queue_family_index{0}, queue_family_index{0}},
        {},
        {}};
    SwapchainConfiguration configuration{
        {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        VK_PRESENT_MODE_FIFO_KHR,
        {800, 600},
        swapchain_image_count{2}};
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    capabilities.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    bool is_old_destroyed = false;
    swapchain_ptr old_swapchain{old_handle, [&](auto) { is_old_destroyed = true; }};
    std::weak_ptr old_observer{old_swapchain};

    SUBCASE("discarded replacement plan releases old swapchain without native calls")
    {
        {
            auto plan = reader_io_vk_create_swapchain_khr<SwapchainEnvironment>(
                std::move(presentation_device),
                configuration,
                capabilities,
                std::move(old_swapchain));
            [[maybe_unused]] auto discarded = plan.build(environment);

            CHECK_FALSE(is_old_destroyed);
            CHECK_FALSE(old_observer.expired());
        }

        CHECK(is_old_destroyed);
        CHECK(old_observer.expired());
        return;
    }

    VkResult create_status = VK_SUCCESS;
    SUBCASE("successful creation destroys old swapchain after call returns") {}
    SUBCASE("failed creation still destroys old swapchain after call returns")
    {
        create_status = VK_ERROR_OUT_OF_DATE_KHR;
    }

    REQUIRE_CALL(
        api, vk_create_swapchain_khr(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_WITH(_2->oldSwapchain == old_handle && !is_old_destroyed)
        .LR_SIDE_EFFECT(*_4 = new_handle)
        .LR_RETURN(create_status);
    std::unique_ptr<trompeloeil::expectation> destroy_new;
    if (create_status == VK_SUCCESS)
    {
        destroy_new =
            NAMED_REQUIRE_CALL(api, vk_destroy_swapchain_khr(device_handle, new_handle, nullptr));
    }

    // Consume the plan immediately so no plan-held copy prolongs the retired predecessor.
    auto plan = reader_io_vk_create_swapchain_khr<SwapchainEnvironment>(
        std::move(presentation_device), configuration, capabilities, std::move(old_swapchain));
    vulkan_result<swapchain_ptr> outcome =
        std::move(plan).build(environment).io().build().sync_wait(lf::lazy_pool{2});

    CHECK(is_old_destroyed);
    CHECK(old_observer.expired());
    if (create_status == VK_SUCCESS)
    {
        REQUIRE(fnfelt::result::is_success(outcome));
        CHECK_EQ(fnfelt::result::get_success(outcome).get(), new_handle);
    }
    else
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        CHECK_EQ(fnfelt::result::get_error(outcome).result, create_status);
    }
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
