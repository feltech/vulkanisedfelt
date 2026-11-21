// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./queue_presentation.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/mock_api.hpp"
#include "./errors.hpp"
#include "./image_acquisition_types.hpp"
#include "./managed_handles_types.hpp"
#include "./queue_presentation_types.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct QueuePresentationEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] presentation plans are lazy and retain image and synchronization owners")
{
    backend::MockApi api;
    QueuePresentationEnvironment environment{std::ref(api)};
    char device_storage{}, queue_storage{}, swapchain_storage{}, semaphore_storage{};
    char fence_storage{}, wait_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    AcquiredSwapchainImage acquired_image{
        {reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}},
        {reinterpret_cast<VkSemaphore>(&semaphore_storage), [](auto) {}},
        {reinterpret_cast<VkFence>(&fence_storage), [](auto) {}},
        swapchain_image_index{2},
        false};
    semaphore_ptr wait_semaphore{reinterpret_cast<VkSemaphore>(&wait_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    std::weak_ptr swapchain_observer{acquired_image.swapchain};
    std::weak_ptr acquisition_semaphore_observer{acquired_image.semaphore};
    std::weak_ptr acquisition_fence_observer{acquired_image.fence};
    std::weak_ptr wait_observer{wait_semaphore};

    {
        // Planning retains every input without releasing acquisition or queuing semaphore waits.
        auto plan = reader_io_vk_queue_present_khr<QueuePresentationEnvironment>(
            std::move(device),
            reinterpret_cast<VkQueue>(&queue_storage),
            std::move(acquired_image),
            std::move(wait_semaphore));
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(swapchain_observer.expired());
        CHECK_FALSE(acquisition_semaphore_observer.expired());
        CHECK_FALSE(acquisition_fence_observer.expired());
        CHECK_FALSE(wait_observer.expired());
    }

    CHECK(device_observer.expired());
    CHECK(swapchain_observer.expired());
    CHECK(acquisition_semaphore_observer.expired());
    CHECK(acquisition_fence_observer.expired());
    CHECK(wait_observer.expired());
}

TEST_CASE("[impure] presentation queues one image and retains successful request owners")
{
    backend::MockApi api;
    QueuePresentationEnvironment environment{std::ref(api)};
    char device_storage{}, queue_storage{}, swapchain_storage{}, semaphore_storage{};
    char fence_storage{}, wait_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    AcquiredSwapchainImage acquired_image{
        {reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}},
        {reinterpret_cast<VkSemaphore>(&semaphore_storage), [](auto) {}},
        {reinterpret_cast<VkFence>(&fence_storage), [](auto) {}},
        swapchain_image_index{3},
        true};
    semaphore_ptr wait_semaphore{reinterpret_cast<VkSemaphore>(&wait_storage), [](auto) {}};
    VkResult status = VK_SUCCESS;
    bool has_wait = true;
    SUBCASE("success with a binary wait") {}
    SUBCASE("suboptimal still queues the image")
    {
        status = VK_SUBOPTIMAL_KHR;
    }
    SUBCASE("success without a semaphore wait")
    {
        has_wait = false;
    }
    SUBCASE("suboptimal without a semaphore wait")
    {
        status = VK_SUBOPTIMAL_KHR;
        has_wait = false;
    }
    if (!has_wait)
    {
        wait_semaphore.reset();
    }
    VkQueue queue = reinterpret_cast<VkQueue>(&queue_storage);
    VkSwapchainKHR swapchain_handle = acquired_image.swapchain.get();
    VkSemaphore wait_handle = wait_semaphore.get();
    std::weak_ptr device_observer{device};
    std::weak_ptr swapchain_observer{acquired_image.swapchain};
    std::weak_ptr acquisition_semaphore_observer{acquired_image.semaphore};
    std::weak_ptr acquisition_fence_observer{acquired_image.fence};
    std::weak_ptr wait_observer{wait_semaphore};
    REQUIRE_CALL(api, vk_queue_present_khr(queue, trompeloeil::_))
        .LR_SIDE_EFFECT({
            // Borrowed native arrays describe exactly one acquired image during this call.
            CHECK_EQ(_2->sType, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
            CHECK(_2->pNext == nullptr);
            CHECK_EQ(_2->waitSemaphoreCount, has_wait ? 1U : 0U);
            if (has_wait)
            {
                CHECK_EQ(_2->pWaitSemaphores[0], wait_handle);
            }
            else
            {
                CHECK(_2->pWaitSemaphores == nullptr);
            }
            CHECK_EQ(_2->swapchainCount, 1U);
            CHECK_EQ(_2->pSwapchains[0], swapchain_handle);
            CHECK_EQ(_2->pImageIndices[0], 3U);
            CHECK(_2->pResults == nullptr);
            CHECK_FALSE(device_observer.expired());
            CHECK_FALSE(swapchain_observer.expired());
        })
        .LR_RETURN(status);

    {
        // The result carries request keepalives; this mock performs no asynchronous native work.
        vulkan_result<QueuedPresentation> outcome =
            reader_io_vk_queue_present_khr<QueuePresentationEnvironment>(
                std::move(device), queue, std::move(acquired_image), std::move(wait_semaphore))
                .build(environment)
                .sync_wait(lf::lazy_pool{2});

        REQUIRE(fnfelt::result::is_success(outcome));
        QueuedPresentation const & presentation = fnfelt::result::get_success(outcome);
        CHECK_EQ(presentation.acquired_image.swapchain.get(), swapchain_handle);
        CHECK_EQ(value_of(presentation.acquired_image.image_index), 3U);
        CHECK(presentation.acquired_image.is_suboptimal);
        CHECK_EQ(presentation.is_suboptimal, status == VK_SUBOPTIMAL_KHR);
        CHECK_EQ(presentation.wait_semaphore.get(), wait_handle);
        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(swapchain_observer.expired());
        CHECK_FALSE(acquisition_semaphore_observer.expired());
        CHECK_FALSE(acquisition_fence_observer.expired());
        CHECK_EQ(wait_observer.expired(), !has_wait);
    }

    CHECK(device_observer.expired());
    CHECK(swapchain_observer.expired());
    CHECK(acquisition_semaphore_observer.expired());
    CHECK(acquisition_fence_observer.expired());
    CHECK(wait_observer.expired());
}

TEST_CASE("[impure] presentation preserves failures while the caller retains original owners")
{
    backend::MockApi api;
    QueuePresentationEnvironment environment{std::ref(api)};
    char device_storage{}, queue_storage{}, swapchain_storage{}, semaphore_storage{};
    char fence_storage{}, wait_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    AcquiredSwapchainImage acquired_image{
        {reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}},
        {reinterpret_cast<VkSemaphore>(&semaphore_storage), [](auto) {}},
        {reinterpret_cast<VkFence>(&fence_storage), [](auto) {}},
        swapchain_image_index{1},
        false};
    semaphore_ptr wait_semaphore{reinterpret_cast<VkSemaphore>(&wait_storage), [](auto) {}};
    VkResult status = VK_ERROR_OUT_OF_DATE_KHR;
    SUBCASE("out of date can still enqueue semaphore waits") {}
    SUBCASE("surface loss can still enqueue semaphore waits")
    {
        status = VK_ERROR_SURFACE_LOST_KHR;
    }
    SUBCASE("exclusive mode loss can still enqueue semaphore waits")
    {
        status = VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT;
    }
    SUBCASE("host allocation failure")
    {
        status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    SUBCASE("device allocation failure")
    {
        status = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    SUBCASE("device loss is not proof of completion")
    {
        status = VK_ERROR_DEVICE_LOST;
    }
    VkQueue queue = reinterpret_cast<VkQueue>(&queue_storage);
    REQUIRE_CALL(api, vk_queue_present_khr(queue, trompeloeil::_)).LR_RETURN(status);

    // Errors do not prove waits completed; the caller keeps originals for later safe cleanup.
    vulkan_result<QueuedPresentation> outcome =
        reader_io_vk_queue_present_khr<QueuePresentationEnvironment>(
            device, queue, acquired_image, wait_semaphore)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK_EQ(fnfelt::result::get_error(outcome).operation, FallibleVulkanOperation::queue_present);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, status);
    CHECK_EQ(device.use_count(), 1);
    CHECK_EQ(acquired_image.swapchain.use_count(), 1);
    CHECK_EQ(acquired_image.semaphore.use_count(), 1);
    CHECK_EQ(acquired_image.fence.use_count(), 1);
    CHECK_EQ(wait_semaphore.use_count(), 1);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
