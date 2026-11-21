// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./render_pass.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/box.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "../sdl/instance_extensions.hpp"
#include "../sdl/video.hpp"
#include "../sdl/vulkan_surface.hpp"
#include "../sdl/window.hpp"
#include "../text/string.hpp"
#include "./errors.hpp"
#include "./extension_types.hpp"
#include "./instance.hpp"
#include "./presentation_device.hpp"
#include "./presentation_device_types.hpp"
#include "./presentation_image_views.hpp"
#include "./presentation_image_views_types.hpp"
#include "./presentation_swapchain_images.hpp"
#include "./presentation_swapchain_images_types.hpp"
#include "./presentation_swapchain_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct RenderPassEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] render pass creation is lazy and retains its device")
{
    backend::MockApi api;
    RenderPassEnvironment environment{std::ref(api)};
    char device_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    std::weak_ptr device_observer{device};

    {
        // Planning and binding an environment do not create a native render pass.
        auto plan = reader_io_vk_create_render_pass<RenderPassEnvironment>(
            std::move(device), VK_FORMAT_B8G8R8A8_UNORM);
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(device_observer.expired());
    }

    CHECK(device_observer.expired());
}

TEST_CASE("[impure] clear and present render passes retain their device through final cleanup")
{
    backend::MockApi api;
    RenderPassEnvironment environment{std::ref(api)};
    char device_storage{}, render_pass_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkRenderPass render_pass_handle = reinterpret_cast<VkRenderPass>(&render_pass_storage);
    int cleanup_step = 0;
    device_ptr device{device_handle, [&](auto) { CHECK_EQ(++cleanup_step, 2); }};
    std::weak_ptr device_observer{device};
    VkRenderPassCreateInfo captured{};
    VkAttachmentDescription attachment{};
    VkSubpassDescription subpass{};
    VkAttachmentReference color_attachment{};
    VkSubpassDependency dependency{};
    REQUIRE_CALL(api, vk_create_render_pass(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(captured = *_2)
        .LR_SIDE_EFFECT(attachment = _2->pAttachments[0])
        .LR_SIDE_EFFECT(subpass = _2->pSubpasses[0])
        .LR_SIDE_EFFECT(color_attachment = _2->pSubpasses[0].pColorAttachments[0])
        .LR_SIDE_EFFECT(dependency = _2->pDependencies[0])
        .SIDE_EFFECT(*_4 = render_pass_handle)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_destroy_render_pass(device_handle, render_pass_handle, nullptr))
        .LR_SIDE_EFFECT(CHECK_EQ(++cleanup_step, 1));

    // Stack-backed attachment and subpass descriptions are captured during the API call.
    vulkan_result<render_pass_ptr> outcome = reader_io_vk_create_render_pass<RenderPassEnvironment>(
                                                 std::move(device), VK_FORMAT_B8G8R8A8_UNORM)
                                                 .build(environment)
                                                 .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(captured.sType, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    CHECK(captured.pNext == nullptr);
    CHECK_EQ(captured.flags, 0U);
    CHECK_EQ(captured.attachmentCount, 1U);
    CHECK_EQ(captured.subpassCount, 1U);
    CHECK_EQ(captured.dependencyCount, 1U);
    CHECK_EQ(attachment.flags, 0U);
    CHECK_EQ(attachment.format, VK_FORMAT_B8G8R8A8_UNORM);
    CHECK_EQ(attachment.samples, VK_SAMPLE_COUNT_1_BIT);
    CHECK_EQ(attachment.loadOp, VK_ATTACHMENT_LOAD_OP_CLEAR);
    CHECK_EQ(attachment.storeOp, VK_ATTACHMENT_STORE_OP_STORE);
    CHECK_EQ(attachment.stencilLoadOp, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    CHECK_EQ(attachment.stencilStoreOp, VK_ATTACHMENT_STORE_OP_DONT_CARE);
    CHECK_EQ(attachment.initialLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    CHECK_EQ(attachment.finalLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    CHECK_EQ(subpass.flags, 0U);
    CHECK_EQ(subpass.pipelineBindPoint, VK_PIPELINE_BIND_POINT_GRAPHICS);
    CHECK_EQ(subpass.inputAttachmentCount, 0U);
    CHECK(subpass.pInputAttachments == nullptr);
    CHECK_EQ(subpass.colorAttachmentCount, 1U);
    CHECK(subpass.pResolveAttachments == nullptr);
    CHECK(subpass.pDepthStencilAttachment == nullptr);
    CHECK_EQ(subpass.preserveAttachmentCount, 0U);
    CHECK(subpass.pPreserveAttachments == nullptr);
    CHECK_EQ(color_attachment.attachment, 0U);
    CHECK_EQ(color_attachment.layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    CHECK_EQ(dependency.srcSubpass, VK_SUBPASS_EXTERNAL);
    CHECK_EQ(dependency.dstSubpass, 0U);
    CHECK_EQ(dependency.srcStageMask, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    CHECK_EQ(dependency.dstStageMask, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    CHECK_EQ(dependency.srcAccessMask, 0U);
    CHECK_EQ(dependency.dstAccessMask, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    CHECK_EQ(dependency.dependencyFlags, 0U);
    render_pass_ptr render_pass = fnfelt::result::get_success(std::move(outcome));
    CHECK_EQ(render_pass.get(), render_pass_handle);
    render_pass_ptr another_owner{render_pass};
    std::weak_ptr render_pass_observer{render_pass};
    render_pass.reset();
    CHECK_EQ(cleanup_step, 0);
    CHECK_FALSE(device_observer.expired());

    another_owner.reset();

    CHECK_EQ(cleanup_step, 2);
    CHECK(render_pass_observer.expired());
    CHECK(device_observer.expired());
}

TEST_CASE("[impure] render pass creation preserves failure without destroying an unacquired pass")
{
    backend::MockApi api;
    RenderPassEnvironment environment{std::ref(api)};
    char device_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    REQUIRE_CALL(api, vk_create_render_pass(device.get(), trompeloeil::_, nullptr, trompeloeil::_))
        .RETURN(VK_ERROR_OUT_OF_DEVICE_MEMORY);

    // A failed call releases its retained device without attempting native destruction.
    vulkan_result<render_pass_ptr> outcome = reader_io_vk_create_render_pass<RenderPassEnvironment>(
                                                 std::move(device), VK_FORMAT_R8G8B8A8_UNORM)
                                                 .build(environment)
                                                 .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(
        fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::create_render_pass);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CHECK(device_observer.expired());
}

TEST_CASE("[integration][windowed] a clear and present render pass uses the selected image format")
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
    presentation_swapchain_images_result images = runtime.run(
        reader_io_create_presentation_swapchain_images<environment_type>(
            fnfelt::result::get_success(device), runtime.resources().window_owner));
    REQUIRE(fnfelt::result::is_success(images));
    presentation_image_views_result views = runtime.run(
        reader_io_create_presentation_image_views<environment_type>(
            fnfelt::result::get_success(std::move(images))));
    REQUIRE(fnfelt::result::is_success(views));
    PresentationSwapchain const & presentation_swapchain =
        fnfelt::result::get_success(views).presentation_swapchain_images.presentation_swapchain;

    // Creation needs no image acquisition, framebuffer or submitted GPU work.
    vulkan_result<render_pass_ptr> outcome = runtime.run(
        reader_io_vk_create_render_pass<environment_type>(
            presentation_swapchain.presentation_device.device,
            presentation_swapchain.configuration.surface_format.format));

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK(fnfelt::result::get_success(outcome) != nullptr);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
