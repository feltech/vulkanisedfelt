// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./api.hpp"

namespace vulkandemo::backend
{
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
