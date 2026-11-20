// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_swapchain_images.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>
#include <functional>
#include <memory>
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
#include "./errors.hpp"
#include "./extension_types.hpp"
#include "./instance.hpp"
#include "./presentation_device.hpp"
#include "./presentation_swapchain_images_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
TEST_CASE("[impure] presentation image setup propagates failures and releases acquired ownership")
{
    backend::MockApi api;
    sdl::detail::TestRuntime runtime{api};
    using environment_type = sdl::detail::TestState<backend::MockApi>;
    char device_storage{}, physical_storage{}, surface_storage{}, window_storage{},
        swapchain_storage{}, image_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(&physical_storage);
    VkSurfaceKHR surface_handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    SDL_Window * window_handle = reinterpret_cast<SDL_Window *>(&window_storage);
    VkSwapchainKHR swapchain_handle = reinterpret_cast<VkSwapchainKHR>(&swapchain_storage);
    VkImage image_handle = reinterpret_cast<VkImage>(&image_storage);
    PresentationDevice presentation_device{
        surface_ptr{surface_handle, [](auto) {}},
        device_ptr{device_handle, [](auto) {}},
        physical_device,
        {queue_family_index{0}, queue_family_index{0}},
        {},
        {}};
    sdl::window_ptr window{window_handle, [](auto) {}};
    VkResult create_status = VK_SUCCESS;
    VkResult query_status = VK_SUCCESS;
    SUBCASE("success") {}
    SUBCASE("creation failure bypasses image query")
    {
        create_status = VK_ERROR_OUT_OF_DATE_KHR;
    }
    SUBCASE("image query failure cleans up created swapchain")
    {
        query_status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    REQUIRE_CALL(api, sdl_vulkan_get_drawable_size(window_handle, trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(*_2 = 64)
        .SIDE_EFFECT(*_3 = 48);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_capabilities_khr(
            physical_device, surface_handle, trompeloeil::_))
        .SIDE_EFFECT(*_3 = VkSurfaceCapabilitiesKHR{1, 4, {64, 48}, {1, 1}, {4096, 4096}, 1, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT})
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical_device, surface_handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_formats_khr(
            physical_device, surface_handle, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr)
        .SIDE_EFFECT(*_4 = VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_present_modes_khr(
            physical_device, surface_handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_surface_present_modes_khr(
            physical_device, surface_handle, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr)
        .SIDE_EFFECT(*_4 = VK_PRESENT_MODE_FIFO_KHR)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_image_format_properties(
            physical_device,
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            0,
            trompeloeil::_))
        .SIDE_EFFECT(*_7 = VkImageFormatProperties{{4096, 4096, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT, 1048576})
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(
        api, vk_create_swapchain_khr(device_handle, trompeloeil::_, nullptr, trompeloeil::_))
        .SIDE_EFFECT(*_4 = swapchain_handle)
        .LR_RETURN(create_status);
    std::unique_ptr<trompeloeil::expectation> count_query, fill_query, destroy;
    int destroy_calls = 0;
    if (create_status == VK_SUCCESS)
    {
        count_query = NAMED_REQUIRE_CALL(
                          api,
                          vk_get_swapchain_images_khr(
                              device_handle, swapchain_handle, trompeloeil::_, nullptr))
                          .SIDE_EFFECT(*_3 = 1)
                          .LR_RETURN(query_status);
        if (query_status == VK_SUCCESS)
        {
            fill_query = NAMED_REQUIRE_CALL(
                             api,
                             vk_get_swapchain_images_khr(
                                 device_handle, swapchain_handle, trompeloeil::_, trompeloeil::_))
                             .WITH(_4 != nullptr)
                             .SIDE_EFFECT(*_4 = image_handle)
                             .RETURN(VK_SUCCESS);
        }
        destroy = NAMED_REQUIRE_CALL(
                      api, vk_destroy_swapchain_khr(device_handle, swapchain_handle, nullptr))
                      .LR_SIDE_EFFECT(++destroy_calls);
    }
    std::weak_ptr device_observer{presentation_device.device};
    std::weak_ptr surface_observer{presentation_device.surface};

    // A temporary plan transfers ownership into the outcome only when all acquisition succeeds.
    presentation_swapchain_images_result outcome = runtime.run(
        reader_io_create_presentation_swapchain_images<environment_type>(
            std::move(presentation_device), std::move(window)));

    if (create_status == VK_SUCCESS && query_status == VK_SUCCESS)
    {
        REQUIRE(fnfelt::result::is_success(outcome));
        PresentationSwapchainImages snapshot = fnfelt::result::get_success(std::move(outcome));
        CHECK_EQ(snapshot.presentation_swapchain.swapchain.get(), swapchain_handle);
        REQUIRE_EQ(snapshot.images.size(), 1U);
        CHECK_EQ(snapshot.images[0], image_handle);
        CHECK_EQ(destroy_calls, 0);
        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(surface_observer.expired());
        snapshot = {};
    }
    else
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        VulkanFailure failure =
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(outcome)).cause;
        CHECK(
            failure.operation ==
            (create_status == VK_SUCCESS ? FallibleVulkanOperation::get_swapchain_images
                                         : FallibleVulkanOperation::create_swapchain));
        CHECK_EQ(failure.result, create_status == VK_SUCCESS ? query_status : create_status);
    }
    CHECK_EQ(destroy_calls, create_status == VK_SUCCESS ? 1 : 0);
    CHECK(device_observer.expired());
    CHECK(surface_observer.expired());
}

TEST_CASE("[integration][windowed] presentation setup retrieves swapchain-owned images")
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

    // Retrieval observes presentable handles; it neither acquires images nor submits GPU work.
    presentation_swapchain_images_result outcome = runtime.run(
        reader_io_create_presentation_swapchain_images<environment_type>(
            fnfelt::result::get_success(device), runtime.resources().window_owner));

    REQUIRE(fnfelt::result::is_success(outcome));
    PresentationSwapchainImages const & snapshot = fnfelt::result::get_success(outcome);
    CHECK(snapshot.presentation_swapchain.swapchain != nullptr);
    CHECK_FALSE(snapshot.images.empty());
    for (VkImage image : snapshot.images)
    {
        CHECK(image != VK_NULL_HANDLE);
    }
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
