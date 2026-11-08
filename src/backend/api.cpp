// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./api.hpp"

namespace vulkandemo::backend
{
void Api::vk_get_device_queue(
    VkDevice device, std::uint32_t queue_family_index, std::uint32_t queue_index, VkQueue * queue)
{
    ::vkGetDeviceQueue(device, queue_family_index, queue_index, queue);
}
VkResult Api::vk_create_device(
    VkPhysicalDevice device,
    VkDeviceCreateInfo const * create_info,
    VkAllocationCallbacks const * allocator,
    VkDevice * output)
{
    return ::vkCreateDevice(device, create_info, allocator, output);
}
void Api::vk_destroy_device(VkDevice device, VkAllocationCallbacks const * allocator)
{
    ::vkDestroyDevice(device, allocator);
}
VkResult Api::vk_enumerate_device_extension_properties(
    VkPhysicalDevice device,
    char const * layer_name,
    std::uint32_t * count,
    VkExtensionProperties * properties)
{
    return ::vkEnumerateDeviceExtensionProperties(device, layer_name, count, properties);
}
void Api::vk_get_physical_device_queue_family_properties(
    VkPhysicalDevice device, std::uint32_t * count, VkQueueFamilyProperties * properties)
{
    ::vkGetPhysicalDeviceQueueFamilyProperties(device, count, properties);
}
void Api::vk_get_physical_device_properties(
    VkPhysicalDevice device, VkPhysicalDeviceProperties * properties)
{
    ::vkGetPhysicalDeviceProperties(device, properties);
}
VkResult Api::vk_enumerate_physical_devices(
    VkInstance instance, std::uint32_t * count, VkPhysicalDevice * devices)
{
    return ::vkEnumeratePhysicalDevices(instance, count, devices);
}
VkResult Api::vk_enumerate_instance_layer_properties(
    std::uint32_t * count, VkLayerProperties * properties)
{
    return ::vkEnumerateInstanceLayerProperties(count, properties);
}
VkResult Api::vk_enumerate_instance_extension_properties(
    char const * layer_name, std::uint32_t * count, VkExtensionProperties * properties)
{
    return ::vkEnumerateInstanceExtensionProperties(layer_name, count, properties);
}
VkResult Api::vk_create_instance(
    VkInstanceCreateInfo const * create_info,
    VkAllocationCallbacks const * allocator,
    VkInstance * instance)
{
    return ::vkCreateInstance(create_info, allocator, instance);
}
void Api::vk_destroy_instance(VkInstance instance, VkAllocationCallbacks const * allocator)
{
    ::vkDestroyInstance(instance, allocator);
}
}  // namespace vulkandemo::backend
