// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain.hpp
 *
 * Deferred swapchain creation and replacement.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "./presentation_device_types.hpp"
#include "./swapchain_configuration_types.hpp"
#include "detail/swapchain.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans an ordinary per-image swapchain, optionally retiring a predecessor swapchain.
 *
 * The device must enable VK_KHR_swapchain. Configuration must be validated against current
 * surface and image-format limits for 2D optimal color-attachment images, one mip, layer and
 * sample. A nonempty @p old_swapchain must be a non-retired swapchain for the same device and
 * surface; otherwise the surface must have no existing swapchain. Calling `vkCreateSwapchainKHR`
 * retires @p old_swapchain unconditionally even when creation fails; never replay a replacement
 * plan with the same retired handle, and release retired owners before retrying from a null
 * predecessor. The returned owner never chains onto @p old_swapchain. Synchronize host access to
 * the surface and swapchains during creation. The borrowed API must outlive all owners; finish
 * GPU work and child cleanup before final release.
 *
 * @param presentation_device Retained logical device, surface and selected queue families.
 * @param configuration Validated ordinary swapchain choices.
 * @param capabilities Current surface capability snapshot.
 * @param old_swapchain Optional non-retired predecessor retained only through creation.
 * @return Reusable ReaderIO providing ownership or the original Vulkan failure.
 */
template <detail::vk_create_swapchain_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_create_swapchain_khr(
    PresentationDevice presentation_device,
    SwapchainConfiguration configuration,
    VkSurfaceCapabilitiesKHR capabilities,
    swapchain_ptr old_swapchain = {})
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing acquisition with retained parents and optional predecessor handoff.
    auto create_swapchain = std::bind_front(
        detail::create_swapchain_reader_io_action<TEnvironment>,
        std::move(presentation_device),
        configuration,
        capabilities,
        std::move(old_swapchain));

    // ReaderIO providing a newly owned swapchain or the unmodified Vulkan failure.
    return reader_io::create<"vk_create_swapchain_khr"_ss, TEnvironment>(
        std::move(create_swapchain));
}
}  // namespace vulkandemo::vulkan
