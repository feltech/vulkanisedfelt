// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file backend/mock_api.hpp
 *
 * Mock Vulkan and SDL commands for driverless tests.
 */
#pragma once

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>

#include <SDL.h>
#include <vulkan/vulkan_core.h>

#include <trompeloeil.hpp>

namespace vulkandemo::backend
{
/** Per-test command expectations. */
struct MockApi
{
    /** SDL initialization expectations. */
    MAKE_MOCK1(sdl_init_sub_system, auto(Uint32)->int);
    /** SDL subsystem release expectations. */
    MAKE_MOCK1(sdl_quit_sub_system, void(Uint32));
    /** SDL error retrieval expectations. */
    MAKE_MOCK0(sdl_get_error, char const *());
    /** Device queue retrieval expectations. */
    MAKE_MOCK4(vk_get_device_queue, void(VkDevice, std::uint32_t, std::uint32_t, VkQueue *));
    /** Device creation expectations. */
    MAKE_MOCK4(
        vk_create_device,
        VkResult(
            VkPhysicalDevice,
            VkDeviceCreateInfo const *,
            VkAllocationCallbacks const *,
            VkDevice *));
    /** Device destruction expectations. */
    MAKE_MOCK2(vk_destroy_device, void(VkDevice, VkAllocationCallbacks const *));
    /** Device extension enumeration expectations. */
    MAKE_MOCK4(
        vk_enumerate_device_extension_properties,
        VkResult(VkPhysicalDevice, char const *, std::uint32_t *, VkExtensionProperties *));
    /** Queue family query expectations. */
    MAKE_MOCK3(
        vk_get_physical_device_queue_family_properties,
        void(VkPhysicalDevice, std::uint32_t *, VkQueueFamilyProperties *));
    /** Physical device property expectations. */
    MAKE_MOCK2(
        vk_get_physical_device_properties, void(VkPhysicalDevice, VkPhysicalDeviceProperties *));
    /** Physical device enumeration expectations. */
    MAKE_MOCK3(
        vk_enumerate_physical_devices, VkResult(VkInstance, std::uint32_t *, VkPhysicalDevice *));
    /** Instance layer enumeration expectations. */
    MAKE_MOCK2(
        vk_enumerate_instance_layer_properties, VkResult(std::uint32_t *, VkLayerProperties *));
    /** Instance extension enumeration expectations. */
    MAKE_MOCK3(
        vk_enumerate_instance_extension_properties,
        VkResult(char const *, std::uint32_t *, VkExtensionProperties *));
    /** Instance creation expectations. */
    MAKE_MOCK3(
        vk_create_instance,
        VkResult(VkInstanceCreateInfo const *, VkAllocationCallbacks const *, VkInstance *));
    /** Instance destruction expectations. */
    MAKE_MOCK2(vk_destroy_instance, void(VkInstance, VkAllocationCallbacks const *));
};
}  // namespace vulkandemo::backend
#endif  // DOCTEST_CONFIG_DISABLE
