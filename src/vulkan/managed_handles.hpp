// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/managed_handles.hpp
 *
 * Shared owners of API handles with observable cleanup.
 */
#pragma once

#include <memory>
#include <utility>

#include <vulkan/vulkan_core.h>

namespace vulkandemo::vulkan
{
/** Shared instance owner. */
using instance_ptr = std::shared_ptr<VkInstance_T>;

/** Shared logical device owner. */
using device_ptr = std::shared_ptr<VkDevice_T>;

/**
 * Owns an instance created with the default allocator.
 *
 * The API must outlive every owner; release owners within the active effect scope.
 *
 * @param instance Handle acquired with the default allocator.
 * @param api Command implementation that remains alive through cleanup.
 * @return Shared owner of the instance.
 */
template <class TApi>
[[nodiscard]] auto make_instance_ptr(VkInstance instance, TApi & api)
{
    return instance_ptr{
        instance, [&api](VkInstance handle) noexcept { api.vk_destroy_instance(handle, nullptr); }};
}
/**
 * Owns a device created with the default allocator, retaining its instance.
 *
 * The API must outlive every owner. GPU work and child cleanup precede final release.
 *
 * @param device Acquired device handle.
 * @param parent Instance from which the device was acquired.
 * @param api Command implementation alive through cleanup.
 * @return Shared device owner.
 */
template <class TApi>
[[nodiscard]] auto make_device_ptr(VkDevice device, instance_ptr parent, TApi & api)
{
    return device_ptr{
        device,
        [parent = std::move(parent), &api](VkDevice handle) mutable noexcept
        {
            api.vk_destroy_device(handle, nullptr);
            // Release the parent now; weak device owners may retain this deleter's control block.
            parent.reset();
        }};
}
}  // namespace vulkandemo::vulkan
