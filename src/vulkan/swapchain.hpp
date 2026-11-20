// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain.hpp
 *
 * Deferred initial swapchain creation.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./presentation_device_types.hpp"
#include "./swapchain_configuration_types.hpp"
#include "detail/swapchain.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans an initial ordinary per-image swapchain with default allocation.
 *
 * The device must enable VK_KHR_swapchain. Configuration must be validated against current
 * surface and image-format limits for 2D optimal color-attachment images, one mip, layer and
 * sample. The native window must have no existing swapchain. This operation uses the surface's
 * current transform, supported alpha, and concurrent sharing only for separate queue families.
 * Synchronize host access to the surface during creation. Release prior successful owners
 * before replaying this initial-creation plan. The borrowed API must outlive all owners;
 * finish GPU work and child cleanup before final release.
 *
 * @param presentation_device Retained logical device, surface and selected queue families.
 * @param configuration Validated ordinary swapchain choices.
 * @param capabilities Current surface capability snapshot.
 * @return Reusable ReaderIO providing ownership or the original Vulkan failure.
 */
template <detail::vk_create_swapchain_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_create_swapchain_khr(
    PresentationDevice presentation_device,
    SwapchainConfiguration configuration,
    VkSurfaceCapabilitiesKHR capabilities)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory providing acquisition with retained parents and stable configuration values.
    auto io_create_swapchain = std::bind_front(
        detail::io_create_swapchain<TEnvironment>,
        std::move(presentation_device),
        configuration,
        capabilities);

    // ReaderIO providing a newly owned swapchain or the unmodified Vulkan failure.
    return reader_io::create<"vk_create_swapchain_khr"_ss, TEnvironment>(
        std::move(io_create_swapchain));
}
}  // namespace vulkandemo::vulkan
