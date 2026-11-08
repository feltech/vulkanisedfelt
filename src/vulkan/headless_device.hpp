// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/headless_device.hpp
 *
 * Deferred headless device and queue setup.
 */
#pragma once

#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./headless_device_types.hpp"
#include "./managed_handles.hpp"
#include "./physical_device_selection.hpp"
#include "detail/headless_device.hpp"

namespace vulkandemo::vulkan
{
/**
 * Selects a headless physical device and creates its logical device and queue.
 *
 * Requests one queue at priority 1 without device extensions or optional features. Each run
 * creates an independent logical device. The returned owner retains its instance; the borrowed
 * API must outlive all plans, outstanding operations and resulting owners. Complete GPU work and
 * release device children before releasing the final device owner.
 *
 * @tparam TEnvironment Execution environment type.
 * @param instance Nonempty owner of a live parent instance.
 * @param required_queue_flags Required queue capabilities.
 * @return Reusable ReaderIO producing an owned device and queue or renderer error.
 */
template <create_headless_device_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_create_headless_device(
    instance_ptr instance, VkQueueFlags required_queue_flags)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing a suitable physical device and queue family, or a discovery failure.
    auto select_physical_device = reader_io_select_headless_physical_device<TEnvironment>(
        std::move(instance), required_queue_flags);

    // Continuation providing logical-device creation and queue retrieval for a suitable device.
    auto create_selected_device = detail::reader_io_create_selected_headless_device<TEnvironment>;

    // ReaderIO providing a complete owned headless device or the first unsuccessful outcome.
    return reader_io::and_then<"create_headless_device"_ss>(
        std::move(select_physical_device), std::move(create_selected_device));
}
}  // namespace vulkandemo::vulkan
