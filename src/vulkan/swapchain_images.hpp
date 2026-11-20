// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_images.hpp
 *
 * Deferred retrieval of swapchain-owned images.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "detail/swapchain_images.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans image retrieval with at most three count/fill attempts.
 *
 * The device must own the nonempty swapchain. Its owner must retain the device and surface.
 * The borrowed API must outlive execution and all owners. Keep the returned ownership alive
 * throughout image use, and finish GPU work before its final release. Images are borrowed
 * from Vulkan and must not be destroyed individually. Exhausted retries return VK_INCOMPLETE.
 *
 * @param device Retained logical device owning the swapchain.
 * @param swapchain Retained swapchain to query.
 * @return Reusable ReaderIO providing images and ownership or the original Vulkan failure.
 */
template <detail::vk_get_swapchain_images_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_get_swapchain_images_khr(
    device_ptr device, swapchain_ptr swapchain)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory providing image retrieval with retained native parents.
    auto io_get_swapchain_images = std::bind_front(
        detail::io_get_swapchain_images<TEnvironment>, std::move(device), std::move(swapchain));

    // ReaderIO providing a complete image snapshot and the owner anchoring its lifetime.
    return reader_io::create<"vk_get_swapchain_images_khr"_ss, TEnvironment>(
        std::move(io_get_swapchain_images));
}
}  // namespace vulkandemo::vulkan
