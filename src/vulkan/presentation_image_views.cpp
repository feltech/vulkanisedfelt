// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_image_views.hpp"

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
#include "./presentation_image_views_types.hpp"
#include "./presentation_swapchain_images.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct PresentationImageViewEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] presentation views preserve image order and clean up failed batches")
{
    backend::MockApi api;
    PresentationImageViewEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{}, first_image{}, second_image{}, third_image{},
        first_view{}, second_view{}, third_view{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    immer::array<VkImage> images{
        reinterpret_cast<VkImage>(&first_image),
        reinterpret_cast<VkImage>(&second_image),
        reinterpret_cast<VkImage>(&third_image)};
    immer::array<VkImageView> expected_views{
        reinterpret_cast<VkImageView>(&first_view),
        reinterpret_cast<VkImageView>(&second_view),
        reinterpret_cast<VkImageView>(&third_view)};
    bool has_last_failure = false;
    bool has_first_failure = false;
    SUBCASE("all views succeed") {}
    SUBCASE("one failure releases the successful views")
    {
        has_last_failure = true;
    }
    SUBCASE("first error is selected in image order")
    {
        has_first_failure = true;
        has_last_failure = true;
    }
    int const expected_view_count =
        3 - static_cast<int>(has_last_failure) - static_cast<int>(has_first_failure);
    std::atomic<int> destroyed_view_count{0};
    std::atomic<bool> is_device_destroyed{false};
    std::atomic<bool> is_swapchain_destroyed{false};
    device_ptr device{device_handle, [&](auto) { is_device_destroyed.store(true); }};
    swapchain_ptr swapchain{
        reinterpret_cast<VkSwapchainKHR>(&swapchain_storage),
        [parent = device, &destroyed_view_count, &is_swapchain_destroyed, expected_view_count](
            auto) mutable
        {
            CHECK_EQ(destroyed_view_count.load(), expected_view_count);
            is_swapchain_destroyed.store(true);
            parent.reset();
        }};
    PresentationSwapchainImages snapshot{};
    snapshot.presentation_swapchain.presentation_device.device = std::move(device);
    snapshot.presentation_swapchain.swapchain = std::move(swapchain);
    snapshot.presentation_swapchain.configuration.surface_format = {
        VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    snapshot.images = images;
    std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
    for (std::size_t index = 0; index < images.size(); ++index)
    {
        VkImage image = images[index];
        VkImageView view = expected_views[index];
        VkResult status = (index == 0 && has_first_failure)
            ? VK_ERROR_OUT_OF_HOST_MEMORY
            : ((index == 2 && has_last_failure) ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS);
        expectations.push_back(
            NAMED_REQUIRE_CALL(
                api, vk_create_image_view(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
                .WITH(_2->image == image && _2->format == VK_FORMAT_R8G8B8A8_UNORM)
                .SIDE_EFFECT(*_4 = view)
                .RETURN(status));
        if (status == VK_SUCCESS)
        {
            expectations.push_back(
                NAMED_REQUIRE_CALL(api, vk_destroy_image_view(device_handle, view, nullptr))
                    .LR_SIDE_EFFECT(destroyed_view_count.fetch_add(1)));
        }
    }

    // Independent creates may complete in any order; the result follows the input image order.
    presentation_image_views_result outcome =
        reader_io_create_presentation_image_views<PresentationImageViewEnvironment>(
            std::move(snapshot))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    if (!has_last_failure)
    {
        REQUIRE(fnfelt::result::is_success(outcome));
        PresentationImageViews result_snapshot = fnfelt::result::get_success(std::move(outcome));
        REQUIRE_EQ(result_snapshot.image_views.size(), images.size());
        for (std::size_t index = 0; index < images.size(); ++index)
        {
            CHECK_EQ(result_snapshot.image_views[index].get(), expected_views[index]);
            CHECK_EQ(result_snapshot.presentation_swapchain_images.images[index], images[index]);
        }
        image_view_ptr retained_view = result_snapshot.image_views[0];
        result_snapshot = {};
        CHECK_EQ(destroyed_view_count.load(), 2);
        CHECK_FALSE(is_swapchain_destroyed.load());
        CHECK_FALSE(is_device_destroyed.load());

        // An individual view remains sufficient to retain all native parents.
        retained_view.reset();
    }
    else
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        VulkanFailure failure =
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(outcome)).cause;
        CHECK_EQ(failure.operation, FallibleVulkanOperation::create_image_view);
        CHECK_EQ(
            failure.result,
            has_first_failure ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_OUT_OF_DEVICE_MEMORY);
    }
    CHECK_EQ(destroyed_view_count.load(), expected_view_count);
    CHECK(is_swapchain_destroyed.load());
    CHECK(is_device_destroyed.load());
}

TEST_CASE("[integration][windowed] presentation image views retain real swapchain images")
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
    std::size_t const image_count = fnfelt::result::get_success(images).images.size();

    // View creation borrows the images without acquiring them or submitting GPU work.
    presentation_image_views_result outcome = runtime.run(
        reader_io_create_presentation_image_views<environment_type>(
            fnfelt::result::get_success(std::move(images))));

    REQUIRE(fnfelt::result::is_success(outcome));
    PresentationImageViews const & snapshot = fnfelt::result::get_success(outcome);
    REQUIRE_EQ(snapshot.image_views.size(), image_count);
    CHECK_FALSE(snapshot.image_views.empty());
    for (image_view_ptr const & view : snapshot.image_views)
    {
        CHECK(view != nullptr);
    }
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
