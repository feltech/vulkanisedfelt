// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file backend/api.hpp
 *
 * Vulkan commands available to effectful actions.
 */
#pragma once

#include <cstdint>

#include <vulkan/vulkan_core.h>

namespace vulkandemo::backend
{
/** Real Vulkan command implementation. */
struct Api
{
    /**
     * Enumerates device extensions.
     *
     * @param device Borrowed physical device.
     * @param layer_name Layer name, or null for global extensions.
     * @param count Capacity on input and property count on output.
     * @param properties Output storage, or null for the count.
     * @return Vulkan enumeration status.
     */
    VkResult vk_enumerate_device_extension_properties(
        VkPhysicalDevice device,
        char const * layer_name,
        std::uint32_t * count,
        VkExtensionProperties * properties);
    /**
     * Retrieves queue family properties.
     *
     * @param device Borrowed physical device.
     * @param count Capacity on input and written count on output.
     * @param properties Output storage, or null for the count.
     */
    void vk_get_physical_device_queue_family_properties(
        VkPhysicalDevice device, std::uint32_t * count, VkQueueFamilyProperties * properties);
    /**
     * Retrieves physical device properties.
     *
     * @param device Borrowed physical device.
     * @param properties Receives device properties.
     */
    void vk_get_physical_device_properties(
        VkPhysicalDevice device, VkPhysicalDeviceProperties * properties);
    /**
     * Enumerates physical devices belonging to an instance.
     *
     * @param instance Instance to query.
     * @param count Capacity on input and device count on output.
     * @param devices Output storage, or null for the count.
     * @return Vulkan enumeration status.
     */
    VkResult vk_enumerate_physical_devices(
        VkInstance instance, std::uint32_t * count, VkPhysicalDevice * devices);
    /**
     * Enumerates instance layers.
     *
     * @param count Capacity on input and property count on output.
     * @param properties Output storage, or null to query the count.
     * @return Vulkan enumeration status.
     */
    VkResult vk_enumerate_instance_layer_properties(
        std::uint32_t * count, VkLayerProperties * properties);
    /**
     * Enumerates instance extensions.
     *
     * @param layer_name Layer name, or null for global extensions.
     * @param count Capacity on input and property count on output.
     * @param properties Output storage, or null to query the count.
     * @return Vulkan enumeration status.
     */
    VkResult vk_enumerate_instance_extension_properties(
        char const * layer_name, std::uint32_t * count, VkExtensionProperties * properties);
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
