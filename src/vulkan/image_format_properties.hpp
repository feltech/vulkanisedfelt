// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/image_format_properties.hpp
 *
 * Deferred image-format support discovery.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "detail/image_format_properties.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a support query for the exact image description.
 *
 * The parent instance and borrowed API must outlive execution. Unsupported formats remain
 * Vulkan failures so the consuming workflow can assign an appropriate domain meaning.
 *
 * @param device Borrowed physical device.
 * @param format Image format.
 * @param type Image dimensionality.
 * @param tiling Image memory layout.
 * @param usage Required image usages.
 * @param flags Image creation flags.
 * @return Reusable ReaderIO providing support limits or the original Vulkan failure.
 */
template <detail::vk_get_physical_device_image_format_properties_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_get_physical_device_image_format_properties(
    VkPhysicalDevice device,
    VkFormat format,
    VkImageType type,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory providing the exact image-format query from the borrowed command API.
    auto io_get_image_format_properties = std::bind_front(
        detail::io_get_image_format_properties<TEnvironment>,
        device,
        format,
        type,
        tiling,
        usage,
        flags);

    // ReaderIO providing an owned snapshot on every execution.
    return reader_io::create<"vk_get_physical_device_image_format_properties"_ss, TEnvironment>(
        std::move(io_get_image_format_properties));
}
}  // namespace vulkandemo::vulkan
