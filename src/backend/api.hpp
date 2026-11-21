// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file backend/api.hpp
 *
 * Vulkan commands available to effectful actions.
 */
#pragma once

#include <vulkan/vulkan_core.h>

namespace vulkandemo::backend
{
/** Real Vulkan command implementation. */
struct Api
{
    /**
     * Resolves an instance or global Vulkan command.
     *
     * @param instance Instance owner, or null for global commands.
     * @param name NUL-terminated command name.
     * @return Command address, or null when unavailable.
     */
    PFN_vkVoidFunction vk_get_instance_proc_addr(VkInstance instance, char const * name);
    /**
     * Creates a Vulkan instance.
     *
     * @param create_info Instance description.
     * @param allocator Allocation callbacks, or null for default allocation.
     * @param instance Receives the new handle on success.
     * @return Vulkan creation status.
     */
    VkResult vk_create_instance(
        VkInstanceCreateInfo const * create_info,
        VkAllocationCallbacks const * allocator,
        VkInstance * instance);
    /**
     * Destroys a Vulkan instance.
     *
     * @param instance Handle to destroy.
     * @param allocator Callbacks matching creation.
     */
    void vk_destroy_instance(VkInstance instance, VkAllocationCallbacks const * allocator);
};
}  // namespace vulkandemo::backend
