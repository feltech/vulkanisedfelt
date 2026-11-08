// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/headless_device.hpp
 *
 * Composition of headless device acquisition and queue retrieval.
 */
#pragma once

#include <concepts>
#include <functional>
#include <optional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "../device.hpp"
#include "../device_queue.hpp"
#include "../errors.hpp"
#include "../headless_device_types.hpp"
#include "../managed_handles.hpp"
#include "../physical_device_selection.hpp"
#include "../physical_device_selection_types.hpp"
#include "../renderer_errors.hpp"
#include "../strong_types.hpp"

namespace vulkandemo::vulkan
{
/** Commands needed to discover and create a headless device with its queue. */
template <class TEnvironment>
concept create_headless_device_environment =
    select_headless_physical_device_environment<TEnvironment> &&
    vk_create_device_environment<TEnvironment> && vk_get_device_queue_environment<TEnvironment>;
}  // namespace vulkandemo::vulkan

namespace vulkandemo::vulkan::detail
{
/**
 * Retains a logical device alongside its borrowed physical device and queue.
 *
 * @param physical_device Selected physical device.
 * @param queue_family_index Selected queue family.
 * @param device Logical device owning the queue.
 * @param queue Borrowed queue handle.
 * @return Owned headless device context.
 */
constexpr auto make_headless_device(
    VkPhysicalDevice physical_device,
    queue_family_index queue_family_index,
    device_ptr device,
    VkQueue queue) -> HeadlessDevice
{
    return {std::move(device), physical_device, queue_family_index, queue};
}

/**
 * Retrieves the created device's queue and preserves its owner.
 *
 * @tparam TEnvironment Execution environment type.
 * @param physical_device Selected physical device.
 * @param queue_family_index Selected queue family.
 * @param device Acquired logical device.
 * @return ReaderIO providing the owned device and borrowed queue.
 */
template <class TEnvironment>
constexpr auto reader_io_get_headless_device_queue(
    VkPhysicalDevice physical_device, queue_family_index queue_family_index, device_ptr device)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing queue zero from the created device's selected family.
    auto retrieve_queue =
        reader_io_vk_get_device_queue<TEnvironment>(device, queue_family_index, queue_index{0});

    // Transformer providing the acquired device owner alongside its borrowed handles.
    auto retain_device = std::bind_front(
        make_headless_device, physical_device, queue_family_index, std::move(device));

    // ReaderIO providing the completed headless device context.
    return reader_io::transform<"retain_headless_device_queue"_ss>(
        std::move(retrieve_queue), std::move(retain_device));
}

/**
 * Creates a logical device and retrieves its requested queue.
 *
 * @tparam TEnvironment Execution environment type.
 * @param selected Suitable physical device with retained instance.
 * @return ReaderIO providing the completed device context or creation failure.
 */
template <class TEnvironment>
constexpr auto reader_io_create_selected_headless_device(SelectedPhysicalDevice selected)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing a device owner for the selected physical device and queue family.
    auto create_device = reader_io_vk_create_device<TEnvironment>(
        std::move(selected.instance), selected.physical_device, selected.queue_family_index);

    // Error transformer providing renderer-domain context for device creation failures.
    auto map_creation_error = [](VulkanFailure failure) -> renderer_error
    { return VulkanOperationFailure{failure, std::nullopt}; };

    // ReaderIO providing the acquired device or a renderer-domain creation failure.
    auto acquire_device = reader_io::transform_error<"headless_device_creation_error"_ss>(
        std::move(create_device), std::move(map_creation_error));

    // Continuation providing the acquired device and its requested queue.
    auto retrieve_queue = std::bind_front(
        reader_io_get_headless_device_queue<TEnvironment>,
        selected.physical_device,
        selected.queue_family_index);

    // ReaderIO providing the complete context, bypassing queue retrieval on errors or stops.
    return reader_io::and_then<"get_headless_device_queue"_ss>(
        std::move(acquire_device), std::move(retrieve_queue));
}
}  // namespace vulkandemo::vulkan::detail
