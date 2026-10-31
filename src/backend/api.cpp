// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./api.hpp"

namespace vulkandemo::backend
{
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
