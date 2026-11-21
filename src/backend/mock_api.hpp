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
    /** Global and instance command lookup expectations. */
    MAKE_MOCK2(vk_get_instance_proc_addr, PFN_vkVoidFunction(VkInstance, char const *));
    /** Command pool creation expectations. */
    MAKE_MOCK4(
        vk_create_command_pool,
        VkResult(
            VkDevice,
            VkCommandPoolCreateInfo const *,
            VkAllocationCallbacks const *,
            VkCommandPool *));
    /** Command pool destruction expectations. */
    MAKE_MOCK3(
        vk_destroy_command_pool, void(VkDevice, VkCommandPool, VkAllocationCallbacks const *));
    /** Framebuffer creation expectations. */
    MAKE_MOCK4(
        vk_create_framebuffer,
        VkResult(
            VkDevice,
            VkFramebufferCreateInfo const *,
            VkAllocationCallbacks const *,
            VkFramebuffer *));
    /** Framebuffer destruction expectations. */
    MAKE_MOCK3(
        vk_destroy_framebuffer, void(VkDevice, VkFramebuffer, VkAllocationCallbacks const *));
    /** Render pass creation expectations. */
    MAKE_MOCK4(
        vk_create_render_pass,
        VkResult(
            VkDevice,
            VkRenderPassCreateInfo const *,
            VkAllocationCallbacks const *,
            VkRenderPass *));
    /** Render pass destruction expectations. */
    MAKE_MOCK3(vk_destroy_render_pass, void(VkDevice, VkRenderPass, VkAllocationCallbacks const *));
    /** Image view creation expectations. */
    MAKE_MOCK4(
        vk_create_image_view,
        VkResult(
            VkDevice, VkImageViewCreateInfo const *, VkAllocationCallbacks const *, VkImageView *));
    /** Image view destruction expectations. */
    MAKE_MOCK3(vk_destroy_image_view, void(VkDevice, VkImageView, VkAllocationCallbacks const *));
    /** Swapchain image query expectations. */
    MAKE_MOCK4(
        vk_get_swapchain_images_khr,
        VkResult(VkDevice, VkSwapchainKHR, std::uint32_t *, VkImage *));
    /** Swapchain creation expectations. */
    MAKE_MOCK4(
        vk_create_swapchain_khr,
        VkResult(
            VkDevice,
            VkSwapchainCreateInfoKHR const *,
            VkAllocationCallbacks const *,
            VkSwapchainKHR *));
    /** Image-format support query expectations. */
    MAKE_MOCK7(
        vk_get_physical_device_image_format_properties,
        VkResult(
            VkPhysicalDevice,
            VkFormat,
            VkImageType,
            VkImageTiling,
            VkImageUsageFlags,
            VkImageCreateFlags,
            VkImageFormatProperties *));
    /** Swapchain destruction expectations. */
    MAKE_MOCK3(
        vk_destroy_swapchain_khr, void(VkDevice, VkSwapchainKHR, VkAllocationCallbacks const *));
    /** Vulkan drawable-size query expectations. */
    MAKE_MOCK3(sdl_vulkan_get_drawable_size, void(SDL_Window *, int *, int *));
    /** Surface present-mode query expectations. */
    MAKE_MOCK4(
        vk_get_physical_device_surface_present_modes_khr,
        VkResult(VkPhysicalDevice, VkSurfaceKHR, std::uint32_t *, VkPresentModeKHR *));
    /** Surface format query expectations. */
    MAKE_MOCK4(
        vk_get_physical_device_surface_formats_khr,
        VkResult(VkPhysicalDevice, VkSurfaceKHR, std::uint32_t *, VkSurfaceFormatKHR *));
    /** Surface capability query expectations. */
    MAKE_MOCK3(
        vk_get_physical_device_surface_capabilities_khr,
        VkResult(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR *));
    /** Queue-family presentation-support expectations. */
    MAKE_MOCK4(
        vk_get_physical_device_surface_support_khr,
        VkResult(VkPhysicalDevice, std::uint32_t, VkSurfaceKHR, VkBool32 *));
    /** SDL Vulkan surface creation expectations. */
    MAKE_MOCK3(sdl_vulkan_create_surface, SDL_bool(SDL_Window *, VkInstance, VkSurfaceKHR *));
    /** Presentation surface destruction expectations. */
    MAKE_MOCK3(
        vk_destroy_surface_khr, void(VkInstance, VkSurfaceKHR, VkAllocationCallbacks const *));
    /** SDL Vulkan extension-query expectations. */
    MAKE_MOCK3(
        sdl_vulkan_get_instance_extensions, SDL_bool(SDL_Window *, unsigned int *, char const **));
    /** SDL window creation expectations. */
    MAKE_MOCK6(sdl_create_window, SDL_Window *(char const *, int, int, int, int, Uint32));
    /** SDL window destruction expectations. */
    MAKE_MOCK1(sdl_destroy_window, void(SDL_Window *));
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
