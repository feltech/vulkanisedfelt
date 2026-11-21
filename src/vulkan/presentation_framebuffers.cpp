// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_framebuffers.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <atomic>
#include <cstddef>
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
#include "../text/string.hpp"
#include "./errors.hpp"
#include "./extension_types.hpp"
#include "./instance.hpp"
#include "./presentation_device.hpp"
#include "./presentation_framebuffers_types.hpp"
#include "./presentation_image_views.hpp"
#include "./presentation_swapchain_images.hpp"
#include "./render_pass.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct PresentationFramebufferEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] presentation framebuffers retain parents and clean up failed batches")
{
    backend::MockApi api;
    PresentationFramebufferEnvironment environment{std::ref(api)};
    char device_storage{}, physical_device_storage{}, pass_storage{}, swapchain_storage{},
        first_view{}, second_view{}, third_view{}, first_framebuffer{}, second_framebuffer{},
        third_framebuffer{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&physical_device_storage);
    VkRenderPass pass_handle = reinterpret_cast<VkRenderPass>(&pass_storage);
    immer::array<VkImageView> views{
        reinterpret_cast<VkImageView>(&first_view),
        reinterpret_cast<VkImageView>(&second_view),
        reinterpret_cast<VkImageView>(&third_view)};
    immer::array<VkFramebuffer> expected_framebuffers{
        reinterpret_cast<VkFramebuffer>(&first_framebuffer),
        reinterpret_cast<VkFramebuffer>(&second_framebuffer),
        reinterpret_cast<VkFramebuffer>(&third_framebuffer)};
    bool has_last_failure = false;
    bool has_first_failure = false;
    VkExtent2D extent{640, 480};
    SUBCASE("all framebuffers succeed at the physical limits") {}
    SUBCASE("one failure releases the successful framebuffers")
    {
        has_last_failure = true;
    }
    SUBCASE("first error is selected in image order")
    {
        has_first_failure = true;
        has_last_failure = true;
    }
    SUBCASE("unsupported width bypasses native creation")
    {
        extent.width = 641;
    }
    SUBCASE("unsupported height bypasses native creation")
    {
        extent.height = 481;
    }
    bool const has_unsupported_extent = extent.width > 640 || extent.height > 480;
    int const expected_framebuffer_count = has_unsupported_extent
        ? 0
        : 3 - static_cast<int>(has_last_failure) - static_cast<int>(has_first_failure);
    std::atomic<int> destroyed_framebuffer_count{0};
    std::atomic<int> destroyed_view_count{0};
    std::atomic<bool> is_pass_destroyed{false};
    std::atomic<bool> is_swapchain_destroyed{false};
    std::atomic<bool> is_device_destroyed{false};
    device_ptr device{device_handle, [&](auto) { is_device_destroyed.store(true); }};
    render_pass_ptr render_pass{
        pass_handle,
        [parent = device,
         &destroyed_framebuffer_count,
         &is_pass_destroyed,
         expected_framebuffer_count](auto) mutable
        {
            CHECK_EQ(destroyed_framebuffer_count.load(), expected_framebuffer_count);
            is_pass_destroyed.store(true);
            parent.reset();
        }};
    swapchain_ptr swapchain{
        reinterpret_cast<VkSwapchainKHR>(&swapchain_storage),
        [parent = device, &destroyed_view_count, &is_swapchain_destroyed](auto) mutable
        {
            CHECK_EQ(destroyed_view_count.load(), 3);
            is_swapchain_destroyed.store(true);
            parent.reset();
        }};
    auto owned_views = immer::array<image_view_ptr>{}.transient();
    for (VkImageView view : views)
    {
        owned_views.push_back(
            image_view_ptr{
                view,
                [parent = swapchain, &destroyed_view_count](auto) mutable
                {
                    destroyed_view_count.fetch_add(1);
                    parent.reset();
                }});
    }
    PresentationImageViews snapshot{};
    snapshot.presentation_swapchain_images.presentation_swapchain.presentation_device.device =
        std::move(device);
    snapshot.presentation_swapchain_images.presentation_swapchain.presentation_device
        .physical_device = physical_device;
    snapshot.presentation_swapchain_images.presentation_swapchain.swapchain = std::move(swapchain);
    snapshot.presentation_swapchain_images.presentation_swapchain.configuration.extent = extent;
    snapshot.image_views = std::move(owned_views).persistent();
    REQUIRE_CALL(api, vk_get_physical_device_properties(physical_device, trompeloeil::_))
        .SIDE_EFFECT(_2->limits.maxFramebufferWidth = 640)
        .SIDE_EFFECT(_2->limits.maxFramebufferHeight = 480);
    std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
    if (!has_unsupported_extent)
    {
        for (std::size_t index = 0; index < views.size(); ++index)
        {
            VkImageView view = views[index];
            VkFramebuffer framebuffer = expected_framebuffers[index];
            VkResult status = (index == 0 && has_first_failure)
                ? VK_ERROR_OUT_OF_HOST_MEMORY
                : ((index == 2 && has_last_failure) ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS);
            expectations.push_back(
                NAMED_REQUIRE_CALL(
                    api,
                    vk_create_framebuffer(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
                    .WITH(
                        _2->pAttachments[0] == view && _2->renderPass == pass_handle &&
                        _2->width == 640 && _2->height == 480)
                    .SIDE_EFFECT(*_4 = framebuffer)
                    .RETURN(status));
            if (status == VK_SUCCESS)
            {
                expectations.push_back(
                    NAMED_REQUIRE_CALL(
                        api, vk_destroy_framebuffer(device_handle, framebuffer, nullptr))
                        .LR_SIDE_EFFECT(destroyed_framebuffer_count.fetch_add(1)));
            }
        }
    }

    // A physical-limit failure prevents creation; independent valid creates preserve image order.
    presentation_framebuffers_result outcome =
        reader_io_create_presentation_framebuffers<PresentationFramebufferEnvironment>(
            std::move(snapshot), std::move(render_pass))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    if (has_unsupported_extent)
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        UnsupportedFramebufferExtent failure =
            std::get<UnsupportedFramebufferExtent>(fnfelt::result::get_error(outcome));
        CHECK_EQ(failure.requested.width, extent.width);
        CHECK_EQ(failure.requested.height, extent.height);
        CHECK_EQ(failure.maximum.width, 640U);
        CHECK_EQ(failure.maximum.height, 480U);
    }
    else if (has_last_failure)
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        VulkanFailure failure =
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(outcome)).cause;
        CHECK_EQ(failure.operation, FallibleVulkanOperation::create_framebuffer);
        CHECK_EQ(
            failure.result,
            has_first_failure ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_OUT_OF_DEVICE_MEMORY);
    }
    else
    {
        REQUIRE(fnfelt::result::is_success(outcome));
        PresentationFramebuffers result_snapshot = fnfelt::result::get_success(std::move(outcome));
        REQUIRE_EQ(result_snapshot.framebuffers.size(), views.size());
        for (std::size_t index = 0; index < views.size(); ++index)
        {
            CHECK_EQ(result_snapshot.framebuffers[index].get(), expected_framebuffers[index]);
        }
        framebuffer_ptr retained_framebuffer = result_snapshot.framebuffers[0];
        result_snapshot = {};
        CHECK_EQ(destroyed_framebuffer_count.load(), 2);
        CHECK_EQ(destroyed_view_count.load(), 2);
        CHECK_FALSE(is_pass_destroyed.load());
        CHECK_FALSE(is_swapchain_destroyed.load());
        CHECK_FALSE(is_device_destroyed.load());

        // One framebuffer independently retains its view, render pass and native parents.
        retained_framebuffer.reset();
    }
    CHECK_EQ(destroyed_framebuffer_count.load(), expected_framebuffer_count);
    CHECK_EQ(destroyed_view_count.load(), 3);
    CHECK(is_pass_destroyed.load());
    CHECK(is_swapchain_destroyed.load());
    CHECK(is_device_destroyed.load());
}

TEST_CASE("[integration][windowed] presentation framebuffers retain real views and render pass")
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
    vulkan_result<render_pass_ptr> render_pass = runtime.run(
        reader_io_vk_create_render_pass<environment_type>(
            presentation_swapchain.presentation_device.device,
            presentation_swapchain.configuration.surface_format.format));
    REQUIRE(fnfelt::result::is_success(render_pass));
    std::size_t const view_count = fnfelt::result::get_success(views).image_views.size();

    // Creating render targets neither acquires images nor submits GPU work.
    presentation_framebuffers_result outcome = runtime.run(
        reader_io_create_presentation_framebuffers<environment_type>(
            fnfelt::result::get_success(std::move(views)),
            fnfelt::result::get_success(std::move(render_pass))));

    REQUIRE(fnfelt::result::is_success(outcome));
    PresentationFramebuffers const & snapshot = fnfelt::result::get_success(outcome);
    REQUIRE_EQ(snapshot.framebuffers.size(), view_count);
    CHECK_FALSE(snapshot.framebuffers.empty());
    for (framebuffer_ptr const & framebuffer : snapshot.framebuffers)
    {
        CHECK(framebuffer != nullptr);
    }
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
