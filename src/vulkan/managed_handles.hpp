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

#include <vulkan/vulkan_core.h>

namespace vulkandemo::vulkan
{
/** Shared instance owner. */
using instance_ptr = std::shared_ptr<VkInstance_T>;

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
}  // namespace vulkandemo::vulkan
