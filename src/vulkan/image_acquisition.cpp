// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./image_acquisition.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include <SDL.h>
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
#include "./fence.hpp"
#include "./fence_types.hpp"
#include "./image_acquisition_types.hpp"
#include "./instance.hpp"
#include "./managed_handles.hpp"
#include "./managed_handles_types.hpp"
#include "./presentation_device.hpp"
#include "./presentation_swapchain_images.hpp"
#include "./presentation_swapchain_images_types.hpp"
#include "./renderer_errors.hpp"
#include "./synchronization_types.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct ImageAcquisitionEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] image acquisition plans are lazy and retain their inputs")
{
    backend::MockApi api;
    ImageAcquisitionEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{}, semaphore_storage{}, fence_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    swapchain_ptr swapchain{reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}};
    semaphore_ptr semaphore{reinterpret_cast<VkSemaphore>(&semaphore_storage), [](auto) {}};
    fence_ptr fence{reinterpret_cast<VkFence>(&fence_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    std::weak_ptr swapchain_observer{swapchain};
    std::weak_ptr semaphore_observer{semaphore};
    std::weak_ptr fence_observer{fence};

    {
        // Planning and binding retain owners without acquiring or scheduling native signals.
        auto plan = reader_io_vk_acquire_next_image_khr<ImageAcquisitionEnvironment>(
            std::move(device),
            std::move(swapchain),
            timeout_nanoseconds{5},
            std::move(semaphore),
            std::move(fence));
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(swapchain_observer.expired());
        CHECK_FALSE(semaphore_observer.expired());
        CHECK_FALSE(fence_observer.expired());
    }

    CHECK(device_observer.expired());
    CHECK(swapchain_observer.expired());
    CHECK(semaphore_observer.expired());
    CHECK(fence_observer.expired());
}

TEST_CASE("[impure] image acquisition preserves availability and native failure statuses")
{
    backend::MockApi api;
    ImageAcquisitionEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{}, semaphore_storage{}, fence_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    swapchain_ptr swapchain{reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}};
    semaphore_ptr semaphore{reinterpret_cast<VkSemaphore>(&semaphore_storage), [](auto) {}};
    fence_ptr fence{reinterpret_cast<VkFence>(&fence_storage), [](auto) {}};
    VkResult status = VK_SUCCESS;
    timeout_nanoseconds timeout{123};
    SUBCASE("success") {}
    SUBCASE("suboptimal still acquires the image")
    {
        status = VK_SUBOPTIMAL_KHR;
    }
    SUBCASE("an immediate unsuccessful poll is not ready")
    {
        status = VK_NOT_READY;
        timeout = timeout_nanoseconds{0};
    }
    SUBCASE("finite timeout is a recoverable acquisition error")
    {
        status = VK_TIMEOUT;
    }
    SUBCASE("out of date is a recoverable Vulkan failure")
    {
        status = VK_ERROR_OUT_OF_DATE_KHR;
    }
    SUBCASE("surface loss preserves its native status")
    {
        status = VK_ERROR_SURFACE_LOST_KHR;
    }
    SUBCASE("device loss preserves its native status")
    {
        status = VK_ERROR_DEVICE_LOST;
    }
    SUBCASE("only a semaphore can be supplied")
    {
        fence.reset();
    }
    SUBCASE("only a fence can be supplied")
    {
        semaphore.reset();
    }
    REQUIRE_CALL(
        api,
        vk_acquire_next_image_khr(
            device.get(),
            swapchain.get(),
            value_of(timeout),
            semaphore.get(),
            fence.get(),
            trompeloeil::_))
        .SIDE_EFFECT(*_6 = 3)
        .LR_RETURN(status);

    // Only successful acquisition publishes the output index, including suboptimal success.
    vulkan_result<AcquiredSwapchainImage> outcome =
        reader_io_vk_acquire_next_image_khr<ImageAcquisitionEnvironment>(
            device, swapchain, timeout, semaphore, fence)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    if (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR)
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        CHECK_EQ(
            fnfelt::result::get_error(outcome).operation,
            FallibleVulkanOperation::acquire_next_image);
        CHECK_EQ(fnfelt::result::get_error(outcome).result, status);
    }
    else
    {
        REQUIRE(fnfelt::result::is_success(outcome));
        AcquiredSwapchainImage const & acquired = fnfelt::result::get_success(outcome);
        CHECK_EQ(value_of(acquired.image_index), 3U);
        CHECK_EQ(acquired.is_suboptimal, status == VK_SUBOPTIMAL_KHR);
        CHECK_EQ(acquired.swapchain, swapchain);
        CHECK_EQ(acquired.semaphore, semaphore);
        CHECK_EQ(acquired.fence, fence);
    }
}

TEST_CASE("[impure] unavailable acquisition bypasses success processing and can retry")
{
    backend::MockApi api;
    ImageAcquisitionEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{}, fence_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    swapchain_ptr swapchain{reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}};
    fence_ptr fence{reinterpret_cast<VkFence>(&fence_storage), [](auto) {}};
    VkResult unavailable_status = VK_TIMEOUT;
    timeout_nanoseconds timeout{5};
    SUBCASE("retry after timeout") {}
    SUBCASE("retry after an unsuccessful immediate poll")
    {
        unavailable_status = VK_NOT_READY;
        timeout = timeout_nanoseconds{0};
    }
    trompeloeil::sequence attempts;
    REQUIRE_CALL(
        api,
        vk_acquire_next_image_khr(
            device.get(), swapchain.get(), value_of(timeout), nullptr, fence.get(), trompeloeil::_))
        .IN_SEQUENCE(attempts)
        .LR_RETURN(unavailable_status);
    REQUIRE_CALL(
        api,
        vk_acquire_next_image_khr(
            device.get(), swapchain.get(), value_of(timeout), nullptr, fence.get(), trompeloeil::_))
        .IN_SEQUENCE(attempts)
        .SIDE_EFFECT(*_6 = 2)
        .RETURN(VK_SUCCESS);
    int initial_success_calls = 0;
    int recovery_calls = 0;

    // Unavailability bypasses success processing; recovery can retry with the unchanged signals.
    auto plan =
        reader_io_vk_acquire_next_image_khr<ImageAcquisitionEnvironment>(
            device, swapchain, timeout, {}, fence)
            .transform(
                [&](AcquiredSwapchainImage acquired)
                {
                    ++initial_success_calls;
                    return acquired;
                })
            .or_else(
                [&](VulkanFailure failure)
                {
                    ++recovery_calls;
                    CHECK_EQ(failure.operation, FallibleVulkanOperation::acquire_next_image);
                    CHECK_EQ(failure.result, unavailable_status);
                    return reader_io_vk_acquire_next_image_khr<ImageAcquisitionEnvironment>(
                        device, swapchain, timeout, {}, fence);
                });
    vulkan_result<AcquiredSwapchainImage> outcome =
        plan.build(environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK_EQ(value_of(fnfelt::result::get_success(outcome).image_index), 2U);
    CHECK_EQ(initial_success_calls, 0);
    CHECK_EQ(recovery_calls, 1);
}

TEST_CASE("[impure] acquired image results retain swapchain and signal owners")
{
    backend::MockApi api;
    ImageAcquisitionEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{}, semaphore_storage{}, fence_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkSwapchainKHR swapchain_handle = reinterpret_cast<VkSwapchainKHR>(&swapchain_storage);
    VkSemaphore semaphore_handle = reinterpret_cast<VkSemaphore>(&semaphore_storage);
    VkFence fence_handle = reinterpret_cast<VkFence>(&fence_storage);
    int device_cleanup_count = 0;
    int swapchain_cleanup_count = 0;
    int signal_cleanup_count = 0;
    std::weak_ptr<VkDevice_T> device_observer;
    std::weak_ptr<std::remove_pointer_t<VkSemaphore>> semaphore_observer;
    std::weak_ptr<std::remove_pointer_t<VkFence>> fence_observer;
    REQUIRE_CALL(
        api,
        vk_acquire_next_image_khr(
            device_handle, swapchain_handle, 5, semaphore_handle, fence_handle, trompeloeil::_))
        .SIDE_EFFECT(*_6 = 2)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_destroy_semaphore(device_handle, semaphore_handle, nullptr))
        .LR_SIDE_EFFECT(++signal_cleanup_count);
    REQUIRE_CALL(api, vk_destroy_fence(device_handle, fence_handle, nullptr))
        .LR_SIDE_EFFECT(++signal_cleanup_count);

    // The completed result is the only remaining owner after the inputs and plan disappear.
    vulkan_result<AcquiredSwapchainImage> outcome = [&]
    {
        device_ptr device{device_handle, [&](auto) { ++device_cleanup_count; }};
        device_observer = device;
        swapchain_ptr swapchain{
            swapchain_handle,
            [parent = device, &swapchain_cleanup_count](auto) mutable
            {
                ++swapchain_cleanup_count;
                parent.reset();
            }};
        semaphore_ptr semaphore = make_semaphore_ptr(semaphore_handle, device, api);
        fence_ptr fence = make_fence_ptr(fence_handle, device, api);
        semaphore_observer = semaphore;
        fence_observer = fence;
        return reader_io_vk_acquire_next_image_khr<ImageAcquisitionEnvironment>(
                   std::move(device),
                   std::move(swapchain),
                   timeout_nanoseconds{5},
                   std::move(semaphore),
                   std::move(fence))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});
    }();
    REQUIRE(fnfelt::result::is_success(outcome));
    AcquiredSwapchainImage acquisition = fnfelt::result::get_success(std::move(outcome));
    AcquiredSwapchainImage another_owner{acquisition};
    acquisition = {};
    CHECK_EQ(signal_cleanup_count, 0);
    CHECK_EQ(swapchain_cleanup_count, 0);
    CHECK_FALSE(device_observer.expired());

    // Mock signaling is complete; dropping the last result releases parents despite weak observers.
    another_owner = {};

    CHECK_EQ(signal_cleanup_count, 2);
    CHECK_EQ(swapchain_cleanup_count, 1);
    CHECK_EQ(device_cleanup_count, 1);
    CHECK(semaphore_observer.expired());
    CHECK(fence_observer.expired());
    CHECK(device_observer.expired());
}

TEST_CASE("[impure] unsuccessful acquisition does not publish retained owners")
{
    backend::MockApi api;
    ImageAcquisitionEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{}, fence_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    swapchain_ptr swapchain{reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}};
    fence_ptr fence{reinterpret_cast<VkFence>(&fence_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    std::weak_ptr swapchain_observer{swapchain};
    std::weak_ptr fence_observer{fence};
    VkResult status = VK_TIMEOUT;
    timeout_nanoseconds timeout{5};
    SUBCASE("timeout") {}
    SUBCASE("not ready")
    {
        status = VK_NOT_READY;
        timeout = timeout_nanoseconds{0};
    }
    SUBCASE("out of date")
    {
        status = VK_ERROR_OUT_OF_DATE_KHR;
    }
    REQUIRE_CALL(
        api,
        vk_acquire_next_image_khr(
            device.get(), swapchain.get(), value_of(timeout), nullptr, fence.get(), trompeloeil::_))
        .LR_RETURN(status);

    // Without acquisition, no image index or pending signal needs a retained owner.
    vulkan_result<AcquiredSwapchainImage> outcome =
        reader_io_vk_acquire_next_image_khr<ImageAcquisitionEnvironment>(
            std::move(device), std::move(swapchain), timeout, {}, std::move(fence))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    CHECK(fnfelt::result::is_error(outcome));
    CHECK(device_observer.expired());
    CHECK(swapchain_observer.expired());
    CHECK(fence_observer.expired());
}

TEST_CASE("[integration][windowed] an acquired image can complete through its acquisition fence")
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
    PresentationSwapchainImages const & snapshot = fnfelt::result::get_success(images);
    device_ptr logical_device = snapshot.presentation_swapchain.presentation_device.device;
    vulkan_result<fence_ptr> fence =
        runtime.run(reader_io_vk_create_fence<environment_type>(logical_device));
    REQUIRE(fnfelt::result::is_success(fence));

    // A finite acquisition timeout is valid even when surface forward progress is not guaranteed.
    vulkan_result<AcquiredSwapchainImage> outcome = runtime.run(
        reader_io_vk_acquire_next_image_khr<environment_type>(
            logical_device,
            snapshot.presentation_swapchain.swapchain,
            timeout_nanoseconds{1'000'000'000},
            {},
            fnfelt::result::get_success(fence)));
    REQUIRE(fnfelt::result::is_success(outcome));
    AcquiredSwapchainImage const & acquired = fnfelt::result::get_success(outcome);

    // Acquisition may return before its signal completes. Wait before assertions or RAII cleanup.
    vulkan_result<FenceWaitStatus> completion = runtime.run(
        reader_io_vk_wait_for_fence<environment_type>(
            logical_device,
            acquired.fence,
            timeout_nanoseconds{std::numeric_limits<std::uint64_t>::max()}));

    REQUIRE(fnfelt::result::is_success(completion));
    CHECK_EQ(fnfelt::result::get_success(completion), FenceWaitStatus::signaled);
    CHECK_LT(value_of(acquired.image_index), snapshot.images.size());
    CHECK_EQ(acquired.swapchain, snapshot.presentation_swapchain.swapchain);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
