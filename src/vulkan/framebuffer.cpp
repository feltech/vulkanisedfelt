// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./framebuffer.hpp"

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
struct FramebufferEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] framebuffer creation is lazy")
{
    backend::MockApi api;
    char device_storage{}, pass_storage{}, view_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    render_pass_ptr render_pass{reinterpret_cast<VkRenderPass>(&pass_storage), [](auto) {}};
    image_view_ptr image_view{reinterpret_cast<VkImageView>(&view_storage), [](auto) {}};

    // Constructing the plan retains its inputs without invoking any commands.
    auto create = reader_io_vk_create_framebuffer<FramebufferEnvironment>(
        device, render_pass, image_view, VkExtent2D{640, 480});

    CHECK_EQ(device.use_count(), 2);
    CHECK_EQ(render_pass.use_count(), 2);
    CHECK_EQ(image_view.use_count(), 2);
}

TEST_CASE("[impure] framebuffer creation retains attachments until the final owner")
{
    backend::MockApi api;
    FramebufferEnvironment environment{std::ref(api)};
    char device_storage{}, pass_storage{}, view_storage{}, framebuffer_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkRenderPass pass_handle = reinterpret_cast<VkRenderPass>(&pass_storage);
    VkImageView view_handle = reinterpret_cast<VkImageView>(&view_storage);
    VkFramebuffer framebuffer_handle = reinterpret_cast<VkFramebuffer>(&framebuffer_storage);
    int cleanup_step = 0;
    device_ptr device{device_handle, [&](auto) { CHECK_EQ(++cleanup_step, 4); }};
    render_pass_ptr render_pass{pass_handle, [&](auto) { CHECK_EQ(++cleanup_step, 3); }};
    image_view_ptr image_view{view_handle, [&](auto) { CHECK_EQ(++cleanup_step, 2); }};
    std::weak_ptr device_observer{device};
    std::weak_ptr pass_observer{render_pass};
    std::weak_ptr view_observer{image_view};
    VkFramebufferCreateInfo captured{};
    VkImageView captured_attachment = VK_NULL_HANDLE;
    REQUIRE_CALL(api, vk_create_framebuffer(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(captured = *_2)
        .LR_SIDE_EFFECT(captured_attachment = _2->pAttachments[0])
        .SIDE_EFFECT(*_4 = framebuffer_handle)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_destroy_framebuffer(device_handle, framebuffer_handle, nullptr))
        .LR_SIDE_EFFECT(CHECK_EQ(++cleanup_step, 1));

    // The attachment pointer is read during the call, before its stack storage expires.
    vulkan_result<framebuffer_ptr> outcome =
        reader_io_vk_create_framebuffer<FramebufferEnvironment>(
            std::move(device), std::move(render_pass), std::move(image_view), VkExtent2D{640, 480})
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(captured.sType, VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
    CHECK(captured.pNext == nullptr);
    CHECK_EQ(captured.flags, 0U);
    CHECK_EQ(captured.renderPass, pass_handle);
    CHECK_EQ(captured.attachmentCount, 1U);
    CHECK_EQ(captured_attachment, view_handle);
    CHECK_EQ(captured.width, 640U);
    CHECK_EQ(captured.height, 480U);
    CHECK_EQ(captured.layers, 1U);
    framebuffer_ptr framebuffer = fnfelt::result::get_success(std::move(outcome));
    CHECK_EQ(framebuffer.get(), framebuffer_handle);
    framebuffer_ptr another_owner{framebuffer};
    std::weak_ptr framebuffer_observer{framebuffer};
    framebuffer.reset();
    CHECK_EQ(cleanup_step, 0);

    another_owner.reset();

    CHECK_EQ(cleanup_step, 4);
    CHECK(framebuffer_observer.expired());
    CHECK(view_observer.expired());
    CHECK(pass_observer.expired());
    CHECK(device_observer.expired());
}

TEST_CASE("[impure] framebuffer creation preserves native failure without acquiring ownership")
{
    backend::MockApi api;
    FramebufferEnvironment environment{std::ref(api)};
    char device_storage{}, pass_storage{}, view_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    render_pass_ptr render_pass{reinterpret_cast<VkRenderPass>(&pass_storage), [](auto) {}};
    image_view_ptr image_view{reinterpret_cast<VkImageView>(&view_storage), [](auto) {}};
    std::weak_ptr view_observer{image_view};
    REQUIRE_CALL(api, vk_create_framebuffer(device.get(), trompeloeil::_, nullptr, trompeloeil::_))
        .RETURN(VK_ERROR_OUT_OF_DEVICE_MEMORY);

    // A failed create releases the captured parents without destroying an unacquired framebuffer.
    vulkan_result<framebuffer_ptr> outcome =
        reader_io_vk_create_framebuffer<FramebufferEnvironment>(
            std::move(device), std::move(render_pass), std::move(image_view), VkExtent2D{640, 480})
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(
        fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::create_framebuffer);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CHECK(view_observer.expired());
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
