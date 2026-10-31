// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/device.hpp
 *
 * Deferred creation of a minimal Vulkan device.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles.hpp"
#include "./strong_types.hpp"
#include "detail/device.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans one device queue at priority 1 without features, extensions or custom allocation.
 *
 * The physical device must belong to parent. Idle and device-child cleanup precede final release.
 * The environment exposes an api reference_wrapper supporting device creation and destruction.
 * Its API must remain alive through execution and final device release.
 *
 * @param parent Instance retained through device destruction.
 * @param physical_device Borrowed physical device belonging to parent.
 * @param queue_family_index Family containing at least one queue.
 * @tparam TEnvironment Capabilities supplied when building the action.
 * @return Reusable ReaderIO producing a device owner or Vulkan error.
 */
template <vk_create_device_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_create_device(
    instance_ptr parent, VkPhysicalDevice physical_device, queue_family_index queue_family_index)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory producing device-acquisition IO from an execution environment.
    auto io_create_device = std::bind_front(
        detail::io_create_device<TEnvironment>,
        std::move(parent),
        physical_device,
        queue_family_index);

    // ReaderIO producing a device owner or acquisition failure.
    return reader_io::create<"vk_create_device"_ss, TEnvironment>(std::move(io_create_device));
}
}  // namespace vulkandemo::vulkan
