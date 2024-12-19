// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

#include <SDL_video.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan_core.h>

#include <gsl/pointers>

#include <strong_type/bicrementable.hpp>
#include <strong_type/convertible_to.hpp>
#include <strong_type/equality.hpp>
#include <strong_type/equality_with.hpp>
#include <strong_type/formattable.hpp>
#include <strong_type/implicitly_convertible_to.hpp>
#include <strong_type/indexed.hpp>
#include <strong_type/ordered.hpp>
#include <strong_type/range.hpp>
#include <strong_type/regular.hpp>
#include <strong_type/semiregular.hpp>
#include <strong_type/type.hpp>

#include "Logger.hpp"

namespace vulkandemo::types
{
/**
 * Returns a lambda that performs a static_cast to the given type.
 *
 * Useful in ranges transformations.
 *
 * @tparam T The type to cast to.
 *
 * @return A lambda that takes any object and returns it cast to T.
 */
template <typename T>
constexpr auto cast_fn()
{
	return [](auto && obj) { return static_cast<T>(std::forward<decltype(obj)>(obj)); };
};

constexpr auto value_of_fn()
{
	return []<typename T>(T && obj) { return std::forward<T>(obj).value_of(); };
}

using SDLWindowPtr = std::shared_ptr<SDL_Window>;
SDLWindowPtr make_window_ptr(SDL_Window * window);

using VulkanInstancePtr = std::shared_ptr<std::remove_pointer_t<VkInstance>>;
VulkanInstancePtr make_instance_ptr(VkInstance instance);

using VulkanSurfacePtr = std::shared_ptr<std::remove_pointer_t<VkSurfaceKHR>>;
VulkanSurfacePtr make_surface_ptr(VulkanInstancePtr instance, VkSurfaceKHR surface);

using VulkanDevicePtr = std::shared_ptr<std::remove_pointer_t<VkDevice>>;
VulkanDevicePtr make_device_ptr(VkDevice device);

using VulkanDebugMessengerPtr = std::shared_ptr<std::remove_pointer_t<VkDebugUtilsMessengerEXT>>;
VulkanDebugMessengerPtr make_debug_messenger_ptr(
	VulkanInstancePtr instance,
	gsl::owner<LoggerPtr *> plogger,
	VkDebugUtilsMessengerEXT messenger);

using VulkanSwapchainPtr = std::shared_ptr<std::remove_pointer_t<VkSwapchainKHR>>;
VulkanSwapchainPtr make_swapchain_ptr(VulkanDevicePtr device, VkSwapchainKHR swapchain);

using VulkanImageViewPtr = std::shared_ptr<std::remove_pointer_t<VkImageView>>;
VulkanImageViewPtr make_image_view_ptr(VulkanDevicePtr device, VkImageView image_view);

using VulkanRenderPassPtr = std::shared_ptr<std::remove_pointer_t<VkRenderPass>>;
VulkanRenderPassPtr make_render_pass_ptr(VulkanDevicePtr device, VkRenderPass render_pass);

using VulkanFramebufferPtr = std::shared_ptr<std::remove_pointer_t<VkFramebuffer>>;
VulkanFramebufferPtr make_framebuffer_ptr(VulkanDevicePtr device, VkFramebuffer framebuffer);

using VulkanCommandPoolPtr = std::shared_ptr<std::remove_pointer_t<VkCommandPool>>;
VulkanCommandPoolPtr make_command_pool_ptr(VulkanDevicePtr device, VkCommandPool command_pool);

using VulkanCommandBuffersPtr = std::shared_ptr<std::vector<VkCommandBuffer>>;
VulkanCommandBuffersPtr make_command_buffers_ptr(
	VulkanDevicePtr device,
	VulkanCommandPoolPtr pool,
	std::vector<VkCommandBuffer> command_buffers);

using VulkanSemaphorePtr = std::shared_ptr<std::remove_pointer_t<VkSemaphore>>;
VulkanSemaphorePtr make_semaphore_ptr(VulkanDevicePtr device, VkSemaphore semaphore);

using VulkanImageIdx = strong::type<
	uint32_t,
	struct TagForVulkanImageIdx,
	strong::regular,
	strong::implicitly_convertible_to<uint32_t>,
	strong::equality,
	strong::equality_with<uint32_t>>;

using VulkanClearColour = strong::type<
	std::array<float, 4>,
	struct TagForRgbaColourFractions,
	strong::regular,
	strong::indexed<>,
	strong::range>;

using VulkanCommandBufferCount = strong::type<
	uint32_t,
	struct TagForVulkanCommandBufferCount,
	strong::regular,
	strong::implicitly_convertible_to<uint32_t, std::size_t>,
	strong::equality,
	strong::equality_with<uint32_t, std::size_t>>;

using VulkanQueueFamilyIdx = strong::type<
	uint32_t,
	struct TagForVulkanQueueFamilyIdx,
	strong::regular,
	strong::implicitly_convertible_to<uint32_t>,
	strong::equality,
	strong::equality_with<uint32_t>,
	strong::bicrementable,
	strong::strongly_ordered>;

using VulkanQueueCount = strong::type<
	uint32_t,
	struct TagForVulkanQueueCount,
	strong::implicitly_convertible_to<uint32_t, std::size_t>,
	strong::equality,
	strong::equality_with<uint32_t, std::size_t>,
	strong::strongly_ordered,
	strong::bicrementable>;

using MapOfVulkanQueueFamilyIdxToVectorOfQueues =
	std::map<VulkanQueueFamilyIdx, std::vector<VkQueue>>;

using AvailableDeviceExtensionNameView = strong::type<
	std::string_view,
	struct TagForAvailableDeviceExtensionNameView,
	strong::regular,
	strong::partially_ordered,
	strong::formattable>;

using DesiredDeviceExtensionNameView = strong::type<
	std::string_view,
	struct TagForDesiredDeviceExtensionNameView,
	strong::regular,
	strong::partially_ordered,
	strong::formattable,
	strong::convertible_to<AvailableDeviceExtensionNameView>>;

using AvailableInstanceExtensionNameCstr = strong::
	type<char const *, struct TagForAvailableInstanceExtensionNameCstr, strong::semiregular>;

using AvailableInstanceExtensionNameView = strong::type<
	std::string_view,
	struct TagForAvailableInstanceExtensionNameView,
	strong::regular,
	strong::partially_ordered,
	strong::formattable>;

using DesiredInstanceExtensionNameView = strong::type<
	std::string_view,
	struct TagForDesiredInstanceExtensionNameView,
	strong::regular,
	strong::partially_ordered,
	strong::formattable,
	strong::convertible_to<AvailableInstanceExtensionNameView>>;

using AvailableInstanceLayerNameCstr =
	strong::type<char const *, struct TagForAvailableInstanceLayerNameCstr, strong::semiregular>;

using AvailableInstanceLayerNameView = strong::type<
	std::string_view,
	struct TagForAvailableInstanceLayerNameView,
	strong::regular,
	strong::partially_ordered,
	strong::formattable>;

using DesiredInstanceLayerNameView = strong::type<
	std::string_view,
	struct TagForDesiredInstanceLayerNameView,
	strong::regular,
	strong::partially_ordered,
	strong::formattable,
	strong::convertible_to<AvailableInstanceLayerNameView>>;
}  // namespace vulkandemo::types