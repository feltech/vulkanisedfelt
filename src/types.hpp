// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell

/**
 * @file types.hpp
 *
 * Resource ownership and strong types used by the Vulkan demo.
 */

#pragma once
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <ranges>
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

#include "./Logger.hpp"

namespace vulkandemo::types
{
/** Shared ownership of an SDL window. */
using SDLWindowPtr = std::shared_ptr<SDL_Window>;
/**
 * Take ownership of a window and destroy it when the last owner is released.
 *
 * @param window Window whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
SDLWindowPtr make_window_ptr(SDL_Window * window);

/** Shared ownership of a Vulkan instance. */
using VulkanInstancePtr = std::shared_ptr<std::remove_pointer_t<VkInstance>>;
/**
 * Take ownership of an instance and destroy it when the last owner is released.
 *
 * @param instance Instance whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanInstancePtr make_instance_ptr(VkInstance instance);

/** Shared ownership of a presentation surface. */
using VulkanSurfacePtr = std::shared_ptr<std::remove_pointer_t<VkSurfaceKHR>>;
/**
 * Take ownership of a surface while retaining its parent instance.
 *
 * @param instance Parent instance kept alive until the surface is destroyed.
 * @param surface Surface whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanSurfacePtr make_surface_ptr(VulkanInstancePtr instance, VkSurfaceKHR surface);

/** Shared ownership of a logical device. */
using VulkanDevicePtr = std::shared_ptr<std::remove_pointer_t<VkDevice>>;
/**
 * Take ownership of a logical device and destroy it when the last owner is released.
 *
 * @param device Device whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanDevicePtr make_device_ptr(VkDevice device);

/** Shared ownership of a debug messenger and its callback logger. */
using VulkanDebugMessengerPtr = std::shared_ptr<std::remove_pointer_t<VkDebugUtilsMessengerEXT>>;
/**
 * Take ownership of a debug messenger and its callback logger storage.
 *
 * @param instance Parent instance kept alive until the messenger is destroyed.
 * @param plogger Heap-allocated callback logger pointer whose ownership is transferred.
 * @param messenger Messenger whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanDebugMessengerPtr make_debug_messenger_ptr(
    VulkanInstancePtr instance,
    gsl::owner<LoggerPtr *> plogger,
    VkDebugUtilsMessengerEXT messenger);

/** Shared ownership of a presentation swapchain. */
using VulkanSwapchainPtr = std::shared_ptr<std::remove_pointer_t<VkSwapchainKHR>>;
/**
 * Take ownership of a swapchain while retaining its device.
 *
 * @param device Parent device retained until destruction.
 * @param swapchain Swapchain whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanSwapchainPtr make_swapchain_ptr(VulkanDevicePtr device, VkSwapchainKHR swapchain);

/** Shared ownership of an image view. */
using VulkanImageViewPtr = std::shared_ptr<std::remove_pointer_t<VkImageView>>;
/**
 * Take ownership of an image view while retaining its device.
 *
 * @param device Parent device retained until destruction.
 * @param image_view Image view whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanImageViewPtr make_image_view_ptr(VulkanDevicePtr device, VkImageView image_view);

/** Shared ownership of a render pass. */
using VulkanRenderPassPtr = std::shared_ptr<std::remove_pointer_t<VkRenderPass>>;
/**
 * Take ownership of a render pass while retaining its device.
 *
 * @param device Parent device retained until destruction.
 * @param render_pass Render pass whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanRenderPassPtr make_render_pass_ptr(VulkanDevicePtr device, VkRenderPass render_pass);

/** Shared ownership of a framebuffer. */
using VulkanFramebufferPtr = std::shared_ptr<std::remove_pointer_t<VkFramebuffer>>;
/**
 * Take ownership of a framebuffer while retaining its device.
 *
 * @param device Parent device retained until destruction.
 * @param framebuffer Framebuffer whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanFramebufferPtr make_framebuffer_ptr(VulkanDevicePtr device, VkFramebuffer framebuffer);

/** Shared ownership of a command pool. */
using VulkanCommandPoolPtr = std::shared_ptr<std::remove_pointer_t<VkCommandPool>>;
/**
 * Take ownership of a command pool while retaining its device.
 *
 * @param device Parent device retained until destruction.
 * @param command_pool Command pool whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanCommandPoolPtr make_command_pool_ptr(VulkanDevicePtr device, VkCommandPool command_pool);

/** Shared ownership of command buffers allocated from one pool. */
using VulkanCommandBuffersPtr = std::shared_ptr<std::vector<VkCommandBuffer>>;
/**
 * Take ownership of command buffers while retaining their device and pool.
 *
 * @param device Device from which the buffers were allocated.
 * @param pool Pool from which the buffers were allocated.
 * @param command_buffers Command buffers whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanCommandBuffersPtr make_command_buffers_ptr(
    VulkanDevicePtr device,
    VulkanCommandPoolPtr pool,
    std::vector<VkCommandBuffer> command_buffers);

/** Shared ownership of a semaphore. */
using VulkanSemaphorePtr = std::shared_ptr<std::remove_pointer_t<VkSemaphore>>;
/**
 * Take ownership of a semaphore while retaining its device.
 *
 * @param device Parent device retained until destruction.
 * @param semaphore Semaphore whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanSemaphorePtr make_semaphore_ptr(VulkanDevicePtr device, VkSemaphore semaphore);

/** Shared ownership of a buffer. */
using VulkanBufferPtr = std::shared_ptr<std::remove_pointer_t<VkBuffer>>;
/**
 * Take ownership of a buffer while retaining its device.
 *
 * @param device Parent device retained until destruction.
 * @param buffer Buffer whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanBufferPtr make_buffer_ptr(VulkanDevicePtr device, VkBuffer buffer);

/** Shared ownership of a device memory allocation. */
using VulkanDeviceMemoryPtr = std::shared_ptr<std::remove_pointer_t<VkDeviceMemory>>;
/**
 * Take ownership of a device memory allocation while retaining its device.
 *
 * @param device Parent device retained until the memory is freed.
 * @param memory Memory allocation whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanDeviceMemoryPtr make_device_memory_ptr(VulkanDevicePtr device, VkDeviceMemory memory);

/** Shared ownership of a pipeline layout. */
using VulkanPipelineLayoutPtr = std::shared_ptr<std::remove_pointer_t<VkPipelineLayout>>;
/**
 * Take ownership of a pipeline layout while retaining its device.
 *
 * @param device Parent device retained until destruction.
 * @param pipeline_layout Pipeline layout whose ownership is transferred.
 * @return Shared owner of the transferred resource.
 */
VulkanPipelineLayoutPtr make_pipeline_layout_ptr(
    VulkanDevicePtr device, VkPipelineLayout pipeline_layout);

/** Index of an image in a swapchain. */
using VulkanImageIdx = strong::type<
    uint32_t,
    struct TagForVulkanImageIdx,
    strong::regular,
    strong::implicitly_convertible_to<uint32_t>,
    strong::equality,
    strong::equality_with<uint32_t>>;

/** Index of a physical device memory type. */
using VulkanMemoryTypeIdx = strong::type<
    uint32_t,
    TagForVulkanImageIdx,
    strong::regular,
    strong::implicitly_convertible_to<uint32_t>,
    strong::equality,
    strong::equality_with<uint32_t>>;

/** Red, green, blue and alpha components used to clear an image. */
using VulkanClearColour = strong::type<
    std::array<float, 4>,
    struct TagForRgbaColourFractions,
    strong::regular,
    strong::indexed<>,
    strong::range>;

/** Number of command buffers to allocate. */
using VulkanCommandBufferCount = strong::type<
    uint32_t,
    struct TagForVulkanCommandBufferCount,
    strong::regular,
    strong::implicitly_convertible_to<uint32_t, std::size_t>,
    strong::equality,
    strong::equality_with<uint32_t, std::size_t>>;

/** Index of a physical device queue family. */
using VulkanQueueFamilyIdx = strong::type<
    uint32_t,
    struct TagForVulkanQueueFamilyIdx,
    strong::regular,
    strong::implicitly_convertible_to<uint32_t>,
    strong::equality,
    strong::equality_with<uint32_t>,
    strong::bicrementable,
    strong::strongly_ordered>;

/** Number of queues requested from a family. */
using VulkanQueueCount = strong::type<
    uint32_t,
    struct TagForVulkanQueueCount,
    strong::implicitly_convertible_to<uint32_t, std::size_t>,
    strong::equality,
    strong::equality_with<uint32_t, std::size_t>,
    strong::strongly_ordered,
    strong::bicrementable,
    strong::formattable>;

/** Queues grouped by the family that provides them. */
using MapOfVulkanQueueFamilyIdxToVectorOfQueues =
    std::map<VulkanQueueFamilyIdx, std::vector<VkQueue>>;

/** Borrowed name of a device extension known to be available. */
using AvailableDeviceExtensionNameView = strong::type<
    std::string_view,
    struct TagForAvailableDeviceExtensionNameView,
    strong::regular,
    strong::partially_ordered,
    strong::formattable>;

/** Borrowed name of a requested device extension. */
using DesiredDeviceExtensionNameView = strong::type<
    std::string_view,
    struct TagForDesiredDeviceExtensionNameView,
    strong::regular,
    strong::partially_ordered,
    strong::formattable,
    strong::convertible_to<AvailableDeviceExtensionNameView>>;

/** Null-terminated name of an available instance extension. */
using AvailableInstanceExtensionNameCstr = strong::
    type<char const *, struct TagForAvailableInstanceExtensionNameCstr, strong::semiregular>;

/** Borrowed name of an available instance extension. */
using AvailableInstanceExtensionNameView = strong::type<
    std::string_view,
    struct TagForAvailableInstanceExtensionNameView,
    strong::regular,
    strong::partially_ordered,
    strong::formattable>;

/** Borrowed name of a requested instance extension. */
using DesiredInstanceExtensionNameView = strong::type<
    std::string_view,
    struct TagForDesiredInstanceExtensionNameView,
    strong::regular,
    strong::partially_ordered,
    strong::formattable,
    strong::convertible_to<AvailableInstanceExtensionNameView>>;

/** Null-terminated name of an available instance layer. */
using AvailableInstanceLayerNameCstr =
    strong::type<char const *, struct TagForAvailableInstanceLayerNameCstr, strong::semiregular>;

/** Borrowed name of an available instance layer. */
using AvailableInstanceLayerNameView = strong::type<
    std::string_view,
    struct TagForAvailableInstanceLayerNameView,
    strong::regular,
    strong::partially_ordered,
    strong::formattable>;

/** Borrowed name of a requested instance layer. */
using DesiredInstanceLayerNameView = strong::type<
    std::string_view,
    struct TagForDesiredInstanceLayerNameView,
    strong::regular,
    strong::partially_ordered,
    strong::formattable,
    strong::convertible_to<AvailableInstanceLayerNameView>>;

template <class Container>
concept ContiguousContainer =
    std::is_trivially_copyable_v<
        std::remove_cv_t<std::remove_reference_t<typename Container::value_type>>> &&
    std::ranges::range<Container> && requires(Container container)
{
    {container.data()}->std::convertible_to<typename Container::pointer>;
    {container.size()}->std::convertible_to<std::size_t>;
};
}  // namespace vulkandemo::types
