// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell

// Conflicts with clang-tidy wrt vulkan handle typedefs:
// ReSharper disable CppParameterMayBeConst
// ReSharper disable CppLocalVariableMayBeConst
#include "types.hpp"

#include <cassert>
#include <memory>
#include <utility>
#include <vector>

#include <gsl/pointers>

#include <SDL_video.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan_core.h>

#include "Logger.hpp"

using namespace std::literals;

namespace vulkandemo::types
{
SDLWindowPtr make_window_ptr(SDL_Window * window)
{
	return {
		window,
		[](SDL_Window * const ptr)
		{
			if (ptr != nullptr)
				SDL_DestroyWindow(ptr);
		}};
}

VulkanInstancePtr make_instance_ptr(VkInstance instance)
{
	return {
		instance,
		[](VkInstance ptr)
		{
			if (ptr != nullptr)
				vkDestroyInstance(ptr, nullptr);
		}};
}

VulkanSurfacePtr make_surface_ptr(VulkanInstancePtr instance, VkSurfaceKHR surface)
{
	return VulkanSurfacePtr{
		surface,
		[instance = std::move(instance)](VkSurfaceKHR ptr)
		{
			if (ptr != nullptr)
				vkDestroySurfaceKHR(instance.get(), ptr, nullptr);
		}};
}

VulkanDevicePtr make_device_ptr(VkDevice device)
{
	return VulkanDevicePtr{
		device,
		[](VkDevice ptr)
		{
			if (ptr != nullptr)
				vkDestroyDevice(ptr, nullptr);
		}};
}

VulkanDebugMessengerPtr make_debug_messenger_ptr(
	VulkanInstancePtr instance,
	gsl::owner<LoggerPtr *> const plogger,
	VkDebugUtilsMessengerEXT messenger)
{
	auto const pvkDestroyDebugUtilsMessengerEXT =  // NOLINT(*-identifier-naming)
												   // NOLINTNEXTLINE(*-reinterpret-cast)
		reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance.get(), "vkDestroyDebugUtilsMessengerEXT"));

	assert(pvkDestroyDebugUtilsMessengerEXT);

	return VulkanDebugMessengerPtr{
		messenger,
		[instance = std::move(instance),
		 plogger,
		 // NOLINTNEXTLINE(*-identifier-naming)
		 pvkDestroyDebugUtilsMessengerEXT](VkDebugUtilsMessengerEXT ptr)
		{
			if (ptr != nullptr)
				pvkDestroyDebugUtilsMessengerEXT(instance.get(), ptr, nullptr);

			delete plogger;
		}};
}

VulkanSwapchainPtr make_swapchain_ptr(VulkanDevicePtr device, VkSwapchainKHR swapchain)
{
	return VulkanSwapchainPtr{
		swapchain,
		[device = std::move(device)](VkSwapchainKHR ptr)
		{
			if (ptr != nullptr)
				vkDestroySwapchainKHR(device.get(), ptr, nullptr);
		}};
}

VulkanImageViewPtr make_image_view_ptr(VulkanDevicePtr device, VkImageView image_view)
{
	return VulkanImageViewPtr{
		image_view,
		[device = std::move(device)](VkImageView ptr)
		{
			if (ptr != nullptr)
				vkDestroyImageView(device.get(), ptr, nullptr);
		}};
}

VulkanRenderPassPtr make_render_pass_ptr(VulkanDevicePtr device, VkRenderPass render_pass)
{
	return VulkanRenderPassPtr{
		render_pass,
		[device = std::move(device)](VkRenderPass ptr)
		{
			if (ptr != nullptr)
				vkDestroyRenderPass(device.get(), ptr, nullptr);
		}};
}

VulkanFramebufferPtr make_framebuffer_ptr(VulkanDevicePtr device, VkFramebuffer framebuffer)
{
	return VulkanFramebufferPtr{
		framebuffer,
		[device = std::move(device)](VkFramebuffer ptr)
		{
			if (ptr != nullptr)
				vkDestroyFramebuffer(device.get(), ptr, nullptr);
		}};
}

VulkanCommandPoolPtr make_command_pool_ptr(VulkanDevicePtr device, VkCommandPool command_pool)
{
	return VulkanCommandPoolPtr{
		command_pool,
		[device = std::move(device)](VkCommandPool ptr)
		{
			if (ptr != nullptr)
				vkDestroyCommandPool(device.get(), ptr, nullptr);
		}};
}

VulkanCommandBuffersPtr make_command_buffers_ptr(
	VulkanDevicePtr device, VulkanCommandPoolPtr pool, std::vector<VkCommandBuffer> command_buffers)
{
	return VulkanCommandBuffersPtr{
		new std::vector<VkCommandBuffer>{std::move(command_buffers)},
		[device = std::move(device),
		 pool = std::move(pool)](gsl::owner<std::vector<VkCommandBuffer> *> buffers)
		{
			vkFreeCommandBuffers(device.get(), pool.get(), buffers->size(), buffers->data());
			delete buffers;
		}};
}

VulkanSemaphorePtr make_semaphore_ptr(VulkanDevicePtr device, VkSemaphore semaphore)
{
	return VulkanSemaphorePtr{
		semaphore,
		[device = std::move(device)](VkSemaphore ptr)
		{
			if (ptr != nullptr)
				vkDestroySemaphore(device.get(), ptr, nullptr);
		}};
}

VulkanBufferPtr make_buffer_ptr(VulkanDevicePtr device, VkBuffer buffer)
{
	return VulkanBufferPtr{
		buffer,
		[device = std::move(device)](VkBuffer ptr)
		{
			if (ptr != nullptr)
				vkDestroyBuffer(device.get(), ptr, nullptr);
		}};
}

VulkanDeviceMemoryPtr make_device_memory_ptr(VulkanDevicePtr device, VkDeviceMemory memory)
{
	return VulkanDeviceMemoryPtr{
		memory,
		[device = std::move(device)](VkDeviceMemory ptr)
		{
			if (ptr != nullptr)
				vkFreeMemory(device.get(), ptr, nullptr);
		}};
}
}  // namespace vulkandemo::types
