// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./image_view.hpp"

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
struct ImageViewEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] image view creation is lazy and retains its inputs")
{
    backend::MockApi api;
    ImageViewEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{}, image_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    swapchain_ptr swapchain{reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    std::weak_ptr swapchain_observer{swapchain};

    {
        // Building an operation retains its inputs without creating a native view.
        auto plan = reader_io_vk_create_image_view<ImageViewEnvironment>(
            std::move(device),
            std::move(swapchain),
            reinterpret_cast<VkImage>(&image_storage),
            VK_FORMAT_B8G8R8A8_UNORM);
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(swapchain_observer.expired());
    }

    CHECK(device_observer.expired());
    CHECK(swapchain_observer.expired());
}

TEST_CASE("[impure] image views retain their image owner until final native cleanup")
{
    backend::MockApi api;
    ImageViewEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{}, image_storage{}, view_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkImage image_handle = reinterpret_cast<VkImage>(&image_storage);
    VkImageView view_handle = reinterpret_cast<VkImageView>(&view_storage);
    int cleanup_step = 0;
    device_ptr device{device_handle, [&](auto) { CHECK_EQ(++cleanup_step, 3); }};
    swapchain_ptr swapchain{
        reinterpret_cast<VkSwapchainKHR>(&swapchain_storage),
        [parent = device, &cleanup_step](auto) mutable
        {
            CHECK_EQ(++cleanup_step, 2);
            parent.reset();
        }};
    std::weak_ptr device_observer{device};
    std::weak_ptr swapchain_observer{swapchain};
    VkImageViewCreateInfo captured{};
    REQUIRE_CALL(api, vk_create_image_view(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(captured = *_2)
        .SIDE_EFFECT(*_4 = view_handle)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_destroy_image_view(device_handle, view_handle, nullptr))
        .LR_SIDE_EFFECT(CHECK_EQ(++cleanup_step, 1));

    // The temporary plan disappears before the returned view's lifetime is checked.
    vulkan_result<image_view_ptr> outcome =
        reader_io_vk_create_image_view<ImageViewEnvironment>(
            std::move(device), std::move(swapchain), image_handle, VK_FORMAT_B8G8R8A8_UNORM)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(captured.sType, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    CHECK(captured.pNext == nullptr);
    CHECK_EQ(captured.flags, 0U);
    CHECK_EQ(captured.image, image_handle);
    CHECK_EQ(captured.viewType, VK_IMAGE_VIEW_TYPE_2D);
    CHECK_EQ(captured.format, VK_FORMAT_B8G8R8A8_UNORM);
    CHECK_EQ(captured.components.r, VK_COMPONENT_SWIZZLE_IDENTITY);
    CHECK_EQ(captured.components.g, VK_COMPONENT_SWIZZLE_IDENTITY);
    CHECK_EQ(captured.components.b, VK_COMPONENT_SWIZZLE_IDENTITY);
    CHECK_EQ(captured.components.a, VK_COMPONENT_SWIZZLE_IDENTITY);
    CHECK_EQ(captured.subresourceRange.aspectMask, VK_IMAGE_ASPECT_COLOR_BIT);
    CHECK_EQ(captured.subresourceRange.baseMipLevel, 0U);
    CHECK_EQ(captured.subresourceRange.levelCount, 1U);
    CHECK_EQ(captured.subresourceRange.baseArrayLayer, 0U);
    CHECK_EQ(captured.subresourceRange.layerCount, 1U);
    image_view_ptr view = fnfelt::result::get_success(std::move(outcome));
    CHECK_EQ(view.get(), view_handle);
    image_view_ptr another_owner{view};
    std::weak_ptr view_observer{view};
    view.reset();
    CHECK_EQ(cleanup_step, 0);
    CHECK_FALSE(device_observer.expired());
    CHECK_FALSE(swapchain_observer.expired());

    another_owner.reset();

    CHECK_EQ(cleanup_step, 3);
    CHECK(view_observer.expired());
    CHECK(device_observer.expired());
    CHECK(swapchain_observer.expired());
}

TEST_CASE("[impure] image view creation preserves failures without destroying an unacquired view")
{
    backend::MockApi api;
    ImageViewEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{}, image_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    swapchain_ptr swapchain{reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    std::weak_ptr swapchain_observer{swapchain};
    REQUIRE_CALL(api, vk_create_image_view(device.get(), trompeloeil::_, nullptr, trompeloeil::_))
        .RETURN(VK_ERROR_OUT_OF_DEVICE_MEMORY);

    // Failed creation returns the native failure and releases the temporary plan's inputs.
    vulkan_result<image_view_ptr> outcome = reader_io_vk_create_image_view<ImageViewEnvironment>(
                                                std::move(device),
                                                std::move(swapchain),
                                                reinterpret_cast<VkImage>(&image_storage),
                                                VK_FORMAT_R8G8B8A8_UNORM)
                                                .build(environment)
                                                .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(
        fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::create_image_view);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CHECK(device_observer.expired());
    CHECK(swapchain_observer.expired());
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
