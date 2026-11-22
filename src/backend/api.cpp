// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./api.hpp"

#include <cstdio>
#include <exception>

#include <SDL_vulkan.h>

namespace vulkandemo::backend
{
void Api::report_fatal_error(vulkan::VulkanFailure failure)
{
    std::fprintf(
        stderr,
        "Fatal Vulkan failure: operation %d returned %d.\n",
        static_cast<int>(failure.operation),
        static_cast<int>(failure.result));
    std::terminate();
}
PFN_vkVoidFunction Api::vk_get_instance_proc_addr(VkInstance instance, char const * name)
{
    return ::vkGetInstanceProcAddr(instance, name);
}
VkResult Api::vk_queue_present_khr(VkQueue queue, VkPresentInfoKHR const * info)
{
    return ::vkQueuePresentKHR(queue, info);
}
VkResult Api::vk_queue_submit(
    VkQueue queue, std::uint32_t count, VkSubmitInfo const * submissions, VkFence fence)
{
    return ::vkQueueSubmit(queue, count, submissions, fence);
}
VkResult Api::vk_acquire_next_image_khr(
    VkDevice device,
    VkSwapchainKHR swapchain,
    std::uint64_t timeout,
    VkSemaphore semaphore,
    VkFence fence,
    std::uint32_t * image_index)
{
    return ::vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, image_index);
}
VkResult Api::vk_wait_for_fences(
    VkDevice device,
    std::uint32_t count,
    VkFence const * fences,
    VkBool32 wait_all,
    std::uint64_t timeout)
{
    return ::vkWaitForFences(device, count, fences, wait_all, timeout);
}
void Api::vk_cmd_begin_render_pass(
    VkCommandBuffer command_buffer, VkRenderPassBeginInfo const * info, VkSubpassContents contents)
{
    ::vkCmdBeginRenderPass(command_buffer, info, contents);
}
void Api::vk_cmd_end_render_pass(VkCommandBuffer command_buffer)
{
    ::vkCmdEndRenderPass(command_buffer);
}
VkResult Api::vk_end_command_buffer(VkCommandBuffer command_buffer)
{
    return ::vkEndCommandBuffer(command_buffer);
}
VkResult Api::vk_begin_command_buffer(
    VkCommandBuffer command_buffer, VkCommandBufferBeginInfo const * info)
{
    return ::vkBeginCommandBuffer(command_buffer, info);
}
VkResult Api::vk_create_semaphore(
    VkDevice device,
    VkSemaphoreCreateInfo const * info,
    VkAllocationCallbacks const * allocator,
    VkSemaphore * semaphore)
{
    return ::vkCreateSemaphore(device, info, allocator, semaphore);
}
void Api::vk_destroy_semaphore(
    VkDevice device, VkSemaphore semaphore, VkAllocationCallbacks const * allocator)
{
    ::vkDestroySemaphore(device, semaphore, allocator);
}
VkResult Api::vk_create_fence(
    VkDevice device,
    VkFenceCreateInfo const * info,
    VkAllocationCallbacks const * allocator,
    VkFence * fence)
{
    return ::vkCreateFence(device, info, allocator, fence);
}
void Api::vk_destroy_fence(VkDevice device, VkFence fence, VkAllocationCallbacks const * allocator)
{
    ::vkDestroyFence(device, fence, allocator);
}
VkResult Api::vk_create_command_pool(
    VkDevice device,
    VkCommandPoolCreateInfo const * info,
    VkAllocationCallbacks const * allocator,
    VkCommandPool * command_pool)
{
    return ::vkCreateCommandPool(device, info, allocator, command_pool);
}
void Api::vk_destroy_command_pool(
    VkDevice device, VkCommandPool command_pool, VkAllocationCallbacks const * allocator)
{
    ::vkDestroyCommandPool(device, command_pool, allocator);
}
VkResult Api::vk_allocate_command_buffers(
    VkDevice device, VkCommandBufferAllocateInfo const * info, VkCommandBuffer * command_buffers)
{
    return ::vkAllocateCommandBuffers(device, info, command_buffers);
}
void Api::vk_free_command_buffers(
    VkDevice device,
    VkCommandPool command_pool,
    std::uint32_t count,
    VkCommandBuffer const * command_buffers)
{
    ::vkFreeCommandBuffers(device, command_pool, count, command_buffers);
}
VkResult Api::vk_create_framebuffer(
    VkDevice device,
    VkFramebufferCreateInfo const * info,
    VkAllocationCallbacks const * allocator,
    VkFramebuffer * framebuffer)
{
    return ::vkCreateFramebuffer(device, info, allocator, framebuffer);
}
void Api::vk_destroy_framebuffer(
    VkDevice device, VkFramebuffer framebuffer, VkAllocationCallbacks const * allocator)
{
    ::vkDestroyFramebuffer(device, framebuffer, allocator);
}
VkResult Api::vk_create_render_pass(
    VkDevice device,
    VkRenderPassCreateInfo const * info,
    VkAllocationCallbacks const * allocator,
    VkRenderPass * render_pass)
{
    return ::vkCreateRenderPass(device, info, allocator, render_pass);
}
void Api::vk_destroy_render_pass(
    VkDevice device, VkRenderPass render_pass, VkAllocationCallbacks const * allocator)
{
    ::vkDestroyRenderPass(device, render_pass, allocator);
}
VkResult Api::vk_create_image_view(
    VkDevice device,
    VkImageViewCreateInfo const * info,
    VkAllocationCallbacks const * allocator,
    VkImageView * view)
{
    return ::vkCreateImageView(device, info, allocator, view);
}
void Api::vk_destroy_image_view(
    VkDevice device, VkImageView view, VkAllocationCallbacks const * allocator)
{
    ::vkDestroyImageView(device, view, allocator);
}
VkResult Api::vk_get_swapchain_images_khr(
    VkDevice device, VkSwapchainKHR swapchain, std::uint32_t * count, VkImage * images)
{
    return ::vkGetSwapchainImagesKHR(device, swapchain, count, images);
}
VkResult Api::vk_create_swapchain_khr(
    VkDevice device,
    VkSwapchainCreateInfoKHR const * info,
    VkAllocationCallbacks const * allocator,
    VkSwapchainKHR * swapchain)
{
    return ::vkCreateSwapchainKHR(device, info, allocator, swapchain);
}
VkResult Api::vk_get_physical_device_image_format_properties(
    VkPhysicalDevice device,
    VkFormat format,
    VkImageType type,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageFormatProperties * properties)
{
    return ::vkGetPhysicalDeviceImageFormatProperties(
        device, format, type, tiling, usage, flags, properties);
}
void Api::vk_destroy_swapchain_khr(
    VkDevice device, VkSwapchainKHR swapchain, VkAllocationCallbacks const * allocator)
{
    ::vkDestroySwapchainKHR(device, swapchain, allocator);
}
void Api::sdl_vulkan_get_drawable_size(SDL_Window * window, int * width, int * height)
{
    ::SDL_Vulkan_GetDrawableSize(window, width, height);
}
Uint32 Api::sdl_get_window_id(SDL_Window * window)
{
    return ::SDL_GetWindowID(window);
}
void Api::sdl_free(void * memory)
{
    ::SDL_free(memory);
}
int Api::sdl_poll_event(SDL_Event * event)
{
    return ::SDL_PollEvent(event);
}
VkResult Api::vk_get_physical_device_surface_present_modes_khr(
    VkPhysicalDevice device, VkSurfaceKHR surface, std::uint32_t * count, VkPresentModeKHR * modes)
{
    return ::vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, count, modes);
}
VkResult Api::vk_get_physical_device_surface_formats_khr(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    std::uint32_t * count,
    VkSurfaceFormatKHR * formats)
{
    return ::vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, count, formats);
}
VkResult Api::vk_get_physical_device_surface_capabilities_khr(
    VkPhysicalDevice device, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR * capabilities)
{
    return ::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, capabilities);
}
VkResult Api::vk_get_physical_device_surface_support_khr(
    VkPhysicalDevice device, std::uint32_t family_index, VkSurfaceKHR surface, VkBool32 * supported)
{
    return ::vkGetPhysicalDeviceSurfaceSupportKHR(device, family_index, surface, supported);
}
SDL_bool Api::sdl_vulkan_create_surface(
    SDL_Window * window, VkInstance instance, VkSurfaceKHR * surface)
{
    return ::SDL_Vulkan_CreateSurface(window, instance, surface);
}
void Api::vk_destroy_surface_khr(
    VkInstance instance, VkSurfaceKHR surface, VkAllocationCallbacks const * allocator)
{
    ::vkDestroySurfaceKHR(instance, surface, allocator);
}
SDL_bool Api::sdl_vulkan_get_instance_extensions(
    SDL_Window * window, unsigned int * count, char const ** names)
{
    return ::SDL_Vulkan_GetInstanceExtensions(window, count, names);
}
SDL_Window * Api::sdl_create_window(
    char const * title, int position_x, int position_y, int width, int height, Uint32 flags)
{
    return ::SDL_CreateWindow(title, position_x, position_y, width, height, flags);
}
void Api::sdl_destroy_window(SDL_Window * window)
{
    ::SDL_DestroyWindow(window);
}
int Api::sdl_init_sub_system(Uint32 flags)
{
    return ::SDL_InitSubSystem(flags);
}
void Api::sdl_quit_sub_system(Uint32 flags)
{
    ::SDL_QuitSubSystem(flags);
}
char const * Api::sdl_get_error()
{
    return ::SDL_GetError();
}
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
void Api::vk_get_physical_device_features2(
    VkPhysicalDevice device, VkPhysicalDeviceFeatures2 * features)
{
    ::vkGetPhysicalDeviceFeatures2(device, features);
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
