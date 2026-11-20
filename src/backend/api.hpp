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
     * Resolves an instance or global Vulkan command.
     *
     * @param instance Instance owner, or null for global commands.
     * @param name NUL-terminated command name.
     * @return Command address, or null when unavailable.
     */
    PFN_vkVoidFunction vk_get_instance_proc_addr(VkInstance instance, char const * name);
    /**
     * Queries support limits for an image description.
     *
     * @param device Borrowed physical device.
     * @param format Image format.
     * @param type Image dimensionality.
     * @param tiling Image memory layout.
     * @param usage Required image usages.
     * @param flags Image creation flags.
     * @param properties Receives supported limits.
     * @return Vulkan query status.
     */
    VkResult vk_get_physical_device_image_format_properties(
        VkPhysicalDevice device,
        VkFormat format,
        VkImageType type,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkImageCreateFlags flags,
        VkImageFormatProperties * properties);
    /**
     * Destroys a swapchain after its GPU work and child resources finish.
     *
     * @param device Logical device owning the swapchain.
     * @param swapchain Swapchain to destroy.
     * @param allocator Callbacks matching creation.
     */
    void vk_destroy_swapchain_khr(
        VkDevice device, VkSwapchainKHR swapchain, VkAllocationCallbacks const * allocator);
    /**
     * Retrieves the Vulkan drawable size in pixels.
     *
     * @param window Vulkan-capable window.
     * @param width Receives horizontal drawable size.
     * @param height Receives vertical drawable size.
     */
    void sdl_vulkan_get_drawable_size(SDL_Window * window, int * width, int * height);
    /**
     * Retrieves supported presentation modes for a surface.
     *
     * @param device Physical device supporting the surface.
     * @param surface Presentation surface from the same instance.
     * @param count Capacity on input and mode count on output.
     * @param modes Output storage, or null to query the count.
     * @return Vulkan enumeration status.
     */
    VkResult vk_get_physical_device_surface_present_modes_khr(
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        std::uint32_t * count,
        VkPresentModeKHR * modes);
    /**
     * Retrieves supported presentation surface formats.
     *
     * @param device Physical device supporting the surface.
     * @param surface Presentation surface from the same instance.
     * @param count Capacity on input and format count on output.
     * @param formats Output storage, or null to query the count.
     * @return Vulkan enumeration status.
     */
    VkResult vk_get_physical_device_surface_formats_khr(
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        std::uint32_t * count,
        VkSurfaceFormatKHR * formats);
    /**
     * Queries presentation surface capabilities.
     *
     * @param device Physical device supporting the surface.
     * @param surface Presentation surface from the same instance.
     * @param capabilities Receives surface capabilities on success.
     * @return Vulkan query status.
     */
    VkResult vk_get_physical_device_surface_capabilities_khr(
        VkPhysicalDevice device, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR * capabilities);
    /**
     * Queries whether a queue family can present to a surface.
     *
     * @param device Physical device from the surface's parent instance.
     * @param family_index Queue-family index.
     * @param surface Presentation surface.
     * @param supported Receives presentation support on success.
     * @return Vulkan query status.
     */
    VkResult vk_get_physical_device_surface_support_khr(
        VkPhysicalDevice device,
        std::uint32_t family_index,
        VkSurfaceKHR surface,
        VkBool32 * supported);
    /**
     * Creates a Vulkan presentation surface for an SDL window.
     *
     * @param window Vulkan-capable window.
     * @param instance Instance with SDL's required extensions enabled.
     * @param surface Receives the created surface.
     * @return SDL_TRUE on success, or SDL_FALSE with an SDL error.
     */
    SDL_bool sdl_vulkan_create_surface(
        SDL_Window * window, VkInstance instance, VkSurfaceKHR * surface);
    /**
     * Destroys a presentation surface.
     *
     * @param instance Instance owning the surface.
     * @param surface Surface to destroy.
     * @param allocator Callbacks matching creation.
     */
    void vk_destroy_surface_khr(
        VkInstance instance, VkSurfaceKHR surface, VkAllocationCallbacks const * allocator);
    /**
     * Retrieves instance extensions required by an SDL Vulkan window.
     *
     * @param window Borrowed Vulkan-capable window.
     * @param count Capacity on input and extension count on output.
     * @param names Receives borrowed names, or null to query the count.
     * @return SDL_TRUE on success, or SDL_FALSE with an SDL error.
     */
    SDL_bool sdl_vulkan_get_instance_extensions(
        SDL_Window * window, unsigned int * count, char const ** names);
    /**
     * Creates an SDL window.
     *
     * @param title NUL-terminated UTF-8 title.
     * @param position_x Horizontal position or SDL positioning constant.
     * @param position_y Vertical position or SDL positioning constant.
     * @param width Horizontal size in screen coordinates.
     * @param height Vertical size in screen coordinates.
     * @param flags SDL window creation flags.
     * @return Window handle, or null on failure.
     */
    SDL_Window * sdl_create_window(
        char const * title, int position_x, int position_y, int width, int height, Uint32 flags);
    /**
     * Destroys an SDL window.
     *
     * @param window Window to destroy.
     */
    void sdl_destroy_window(SDL_Window * window);
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
