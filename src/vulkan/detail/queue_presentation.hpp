// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/queue_presentation.hpp
 *
 * Native presentation requests with retained image resources.
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/result.hpp>

#include "../../backend/detail/api.hpp"
#include "../errors.hpp"
#include "../image_acquisition_types.hpp"
#include "../managed_handles_types.hpp"
#include "../queue_presentation_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to queue an acquired image for presentation. */
template <class TEnvironment>
concept vk_queue_present_environment = backend::detail::api_environment<TEnvironment> &&
    requires(TEnvironment environment, VkQueue queue, VkPresentInfoKHR const * info)
{
    {environment.api.get().vk_queue_present_khr(queue, info)}->std::same_as<VkResult>;
};

/**
 * Executes presentation as an IO action retaining its image and signal owners.
 *
 * @param api Borrowed command implementation.
 * @param device Retained logical device.
 * @param queue Presentation-capable queue for the target surface.
 * @param acquired_image Image and acquisition signal owners.
 * @param wait_semaphore Optional binary rendering-completion semaphore.
 * @return Queued request keepalives or the original Vulkan failure.
 */
template <class TApi>
auto queue_present_io_action(
    std::reference_wrapper<TApi> api,
    device_ptr device,
    VkQueue queue,
    AcquiredSwapchainImage acquired_image,
    semaphore_ptr wait_semaphore)
    -> vulkan_result<QueuedPresentation> pre(device != nullptr && queue != nullptr)
        pre(acquired_image.swapchain != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<QueuedPresentation>;
    VkSwapchainKHR swapchain = acquired_image.swapchain.get();
    std::uint32_t image_index = value_of(acquired_image.image_index);
    VkSemaphore wait_handle = wait_semaphore.get();
    VkPresentInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = wait_semaphore ? 1 : 0;
    info.pWaitSemaphores = wait_semaphore ? &wait_handle : nullptr;
    info.swapchainCount = 1;
    info.pSwapchains = &swapchain;
    info.pImageIndices = &image_index;
    // One swapchain has the same status as the overall request, so no result array is needed.
    VkResult status = api.get().vk_queue_present_khr(queue, &info);
    if (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR)
    {
        // Presentation rejection can still enqueue waits; the caller retains original owners.
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::queue_present, status});
    }
    return result::success<result_type>(QueuedPresentation{
        std::move(device),
        std::move(acquired_image),
        std::move(wait_semaphore),
        status == VK_SUBOPTIMAL_KHR});
}

/**
 * Provides a presentation request from borrowed command capabilities.
 *
 * @param device Retained logical device.
 * @param queue Presentation-capable queue for the target surface.
 * @param acquired_image Retained image index and acquisition owners.
 * @param wait_semaphore Optional retained binary wait semaphore.
 * @param environment Borrowed command implementation.
 * @return Deferred native presentation request.
 */
template <class TEnvironment>
constexpr auto io_queue_present(
    device_ptr device,
    VkQueue queue,
    AcquiredSwapchainImage acquired_image,
    semaphore_ptr wait_semaphore,
    TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing native presentation while retaining the image and its synchronization.
    auto present = std::bind_front(
        queue_present_io_action<api_type>,
        environment.api,
        std::move(device),
        queue,
        std::move(acquired_image),
        std::move(wait_semaphore));

    // IO providing request keepalives, not a guarantee that presentation has completed.
    return io::create<"vk_queue_present_khr"_ss>(std::move(present));
}
}  // namespace vulkandemo::vulkan::detail
