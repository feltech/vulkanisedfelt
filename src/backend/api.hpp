// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file backend/api.hpp
 *
 * Vulkan and SDL commands available to effectful actions.
 */
#pragma once

#include <cstdint>

#include <SDL.h>
#include <vulkan/vulkan_core.h>

namespace vulkandemo::backend
{
/** Real command implementation. */
struct Api
{
    /**
     * Acquires references to SDL subsystems.
     *
     * @param flags Subsystems to initialize.
     * @return Zero on success, or a negative error status.
     */
    int sdl_init_sub_system(Uint32 flags);
    /**
     * Releases references to SDL subsystems.
     *
     * @param flags Subsystems to release.
     */
    void sdl_quit_sub_system(Uint32 flags);
    /**
     * Reads the calling thread's last SDL error.
     *
     * @return Borrowed error text, valid until another SDL operation changes it.
     */
    char const * sdl_get_error();
    /**
     * Retrieves a queue created with a logical device.
     *
     * @param device Logical device owning the queue.
     * @param queue_family_index Family requested during device creation.
     * @param queue_index Index within the family's requested queues.
     * @param queue Receives the borrowed queue handle.
     */
    void vk_get_device_queue(
        VkDevice device,
        std::uint32_t queue_family_index,
        std::uint32_t queue_index,
        VkQueue * queue);
    /**
     * Creates a logical device.
     *
     * @param device Borrowed physical device.
     * @param create_info Device description.
     * @param allocator Callbacks, or null for default allocation.
     * @param output Receives the created device.
     * @return Vulkan creation status.
     */
    VkResult vk_create_device(
        VkPhysicalDevice device,
        VkDeviceCreateInfo const * create_info,
        VkAllocationCallbacks const * allocator,
        VkDevice * output);
    /**
     * Destroys a logical device.
     *
     * @param device Device to destroy.
     * @param allocator Callbacks matching creation.
     */
    void vk_destroy_device(VkDevice device, VkAllocationCallbacks const * allocator);
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
