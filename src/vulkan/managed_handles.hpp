// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/managed_handles.hpp
 *
 * Shared owners of API handles with observable cleanup.
 */
#pragma once

#include <functional>
#include <memory>
#include <utility>

#include <vulkan/vulkan_core.h>

#include "../execution/managed_resources.hpp"
#include "../sdl/managed_handles_types.hpp"
#include "./managed_handles_types.hpp"
#include "detail/managed_handles.hpp"

namespace vulkandemo::vulkan
{
/**
 * Owns a binary semaphore while retaining its logical device.
 *
 * Keep the borrowed API alive through cleanup. Complete all submitted batches using the
 * semaphore and externally synchronize host access before final release.
 *
 * @param semaphore Acquired semaphore using the default allocator.
 * @param device Logical device owning the semaphore.
 * @param api Command implementation alive through cleanup.
 * @return Shared semaphore owner retaining its device.
 */
template <class TApi>
[[nodiscard]] auto make_semaphore_ptr(VkSemaphore semaphore, device_ptr device, TApi & api)
    -> semaphore_ptr pre(semaphore != VK_NULL_HANDLE) pre(device != nullptr)
{
    return semaphore_ptr{
        semaphore,
        [device = std::move(device), &api](VkSemaphore handle) mutable noexcept
        {
            api.vk_destroy_semaphore(device.get(), handle, nullptr);
            // Weak semaphore observers must not prolong the device after native cleanup.
            device.reset();
        }};
}

/**
 * Owns a command pool while retaining its logical device.
 *
 * Keep the borrowed API alive through cleanup. Complete submitted commands before final
 * release and externally synchronize all host access to this pool and its command buffers.
 *
 * @param command_pool Acquired pool using the default allocator.
 * @param device Logical device owning the pool.
 * @param api Command implementation alive through cleanup.
 * @return Shared pool owner retaining its device.
 */
template <class TApi>
[[nodiscard]] auto make_command_pool_ptr(VkCommandPool command_pool, device_ptr device, TApi & api)
    -> command_pool_ptr pre(command_pool != VK_NULL_HANDLE) pre(device != nullptr)
{
    return command_pool_ptr{
        command_pool,
        [device = std::move(device), &api](VkCommandPool handle) mutable noexcept
        {
            api.vk_destroy_command_pool(device.get(), handle, nullptr);
            // Weak pool observers must not prolong the device after native cleanup.
            device.reset();
        }};
}

/**
 * Owns a command buffer while retaining its pool and device.
 *
 * The pool must belong to the device. Keep the borrowed API alive through cleanup, finish
 * submitted commands and externally synchronize all host access to the pool and its buffers.
 *
 * @param command_buffer Allocated command buffer.
 * @param device Logical device owning the pool.
 * @param command_pool Pool from which the buffer was allocated.
 * @param api Command implementation alive through cleanup.
 * @return Shared buffer owner retaining its parents.
 */
template <class TApi>
[[nodiscard]] auto make_command_buffer_ptr(
    VkCommandBuffer command_buffer, device_ptr device, command_pool_ptr command_pool, TApi & api)
    -> command_buffer_ptr pre(command_buffer != VK_NULL_HANDLE)
        pre(device != nullptr && command_pool != nullptr)
{
    return command_buffer_ptr{
        command_buffer,
        [device = std::move(device), command_pool = std::move(command_pool), &api](
            VkCommandBuffer handle) mutable noexcept
        {
            api.vk_free_command_buffers(device.get(), command_pool.get(), 1, &handle);
            // Weak buffer observers must not prolong parents after native cleanup.
            command_pool.reset();
            device.reset();
        }};
}

/**
 * Owns a framebuffer while retaining its attachment, render pass and device.
 *
 * The borrowed API must outlive every owner. Finish submitted commands using the framebuffer
 * and synchronize host access before final release. All parents must belong to the device.
 *
 * @param framebuffer Acquired framebuffer using the default allocator.
 * @param device Logical device owning the framebuffer.
 * @param render_pass Compatible render pass.
 * @param image_view Color attachment view.
 * @param api Command implementation alive through cleanup.
 * @return Shared framebuffer owner retaining its parents.
 */
template <class TApi>
[[nodiscard]] auto make_framebuffer_ptr(
    VkFramebuffer framebuffer,
    device_ptr device,
    render_pass_ptr render_pass,
    image_view_ptr image_view,
    TApi & api) -> framebuffer_ptr pre(framebuffer != VK_NULL_HANDLE)
    pre(device != nullptr && render_pass != nullptr && image_view != nullptr)
{
    return framebuffer_ptr{
        framebuffer,
        [device = std::move(device),
         render_pass = std::move(render_pass),
         image_view = std::move(image_view),
         &api](VkFramebuffer handle) mutable noexcept
        {
            api.vk_destroy_framebuffer(device.get(), handle, nullptr);
            // Weak framebuffer observers must not prolong parents after native cleanup.
            image_view.reset();
            render_pass.reset();
            device.reset();
        }};
}

/**
 * Owns a render pass while retaining its logical device.
 *
 * The borrowed API must outlive every owner. Finish submitted commands using the render
 * pass and synchronize host access before final release.
 *
 * @param render_pass Acquired render pass using the default allocator.
 * @param device Logical device owning the render pass.
 * @param api Command implementation alive through cleanup.
 * @return Shared render pass owner retaining its device.
 */
template <class TApi>
[[nodiscard]] auto make_render_pass_ptr(VkRenderPass render_pass, device_ptr device, TApi & api)
    -> render_pass_ptr pre(render_pass != VK_NULL_HANDLE) pre(device != nullptr)
{
    return render_pass_ptr{
        render_pass,
        [device = std::move(device), &api](VkRenderPass handle) mutable noexcept
        {
            api.vk_destroy_render_pass(device.get(), handle, nullptr);
            // Weak render pass observers must not prolong the device after native cleanup.
            device.reset();
        }};
}

/**
 * Owns an image view while retaining its swapchain and logical device.
 *
 * The borrowed API must outlive every owner. Finish GPU work and synchronize host access
 * before final release. The image must belong to the retained swapchain and device.
 *
 * @param view Acquired image view using the default allocator.
 * @param device Logical device owning the view.
 * @param swapchain Swapchain owning the viewed image.
 * @param api Command implementation alive through cleanup.
 * @return Shared view owner retaining its parents.
 */
template <class TApi>
[[nodiscard]] auto make_image_view_ptr(
    VkImageView view, device_ptr device, swapchain_ptr swapchain, TApi & api) -> image_view_ptr
    pre(view != VK_NULL_HANDLE) pre(device != nullptr && swapchain != nullptr)
{
    return image_view_ptr{
        view,
        [device = std::move(device), swapchain = std::move(swapchain), &api](
            VkImageView handle) mutable noexcept
        {
            api.vk_destroy_image_view(device.get(), handle, nullptr);
            // Weak view observers must not prolong parents after native cleanup.
            swapchain.reset();
            device.reset();
        }};
}

/**
 * Owns a swapchain while retaining its logical device and presentation surface.
 *
 * The borrowed API must outlive every owner. Finish GPU work and release swapchain children
 * before final release. Synchronize final destruction against other host access. Cleanup may
 * run on any thread; releasing the surface can dispatch its own cleanup to main.
 *
 * @param swapchain Acquired handle using the default allocator.
 * @param device Logical device owning the swapchain.
 * @param surface Presentation surface associated with the swapchain.
 * @param api Command implementation alive through cleanup.
 * @return Shared swapchain owner retaining both parents.
 */
template <class TApi>
[[nodiscard]] auto make_swapchain_ptr(
    VkSwapchainKHR swapchain, device_ptr device, surface_ptr surface, TApi & api) -> swapchain_ptr
    pre(swapchain != VK_NULL_HANDLE) pre(device != nullptr && surface != nullptr)
{
    return swapchain_ptr{
        swapchain,
        [device = std::move(device), surface = std::move(surface), &api](
            VkSwapchainKHR handle) mutable noexcept
        {
            api.vk_destroy_swapchain_khr(device.get(), handle, nullptr);
            // Weak swapchain observers must not prolong either parent after native cleanup.
            device.reset();
            surface.reset();
        }};
}

/**
 * Owns a surface whose final release destroys it on main before its parents.
 *
 * The borrowed API and serviced context must outlive every owner. Surface users
 * and dependent swapchains must finish before final release.
 *
 * @tparam TApi Command implementation type.
 * @param surface Acquired surface using the default allocator.
 * @param instance Instance owning the surface.
 * @param window Window associated with the surface.
 * @param api Command implementation alive through cleanup.
 * @param main_context Serviced main-thread context alive through cleanup.
 * @return Shared surface owner retaining both parents.
 */
template <class TApi>
[[nodiscard]] auto make_surface_ptr(
    VkSurfaceKHR surface,
    instance_ptr instance,
    sdl::window_ptr window,
    TApi & api,
    lf::worker_context * main_context) noexcept -> surface_ptr pre(surface != VK_NULL_HANDLE)
    pre(instance != nullptr) pre(window != nullptr)
{
    return execution::make_main_thread_ptr(
        surface,
        main_context,
        std::bind_front(
            detail::destroy_surface<TApi>, std::ref(api), std::move(instance), std::move(window)));
}

/**
 * Owns an instance created with the default allocator.
 *
 * The API must outlive every owner; release owners within the active effect scope.
 *
 * @param instance Handle acquired with the default allocator.
 * @param api Command implementation that remains alive through cleanup.
 * @return Shared owner of the instance.
 */
template <class TApi>
[[nodiscard]] auto make_instance_ptr(VkInstance instance, TApi & api) -> instance_ptr
{
    return instance_ptr{
        instance, [&api](VkInstance handle) noexcept { api.vk_destroy_instance(handle, nullptr); }};
}
/**
 * Owns a device created with the default allocator, retaining its instance.
 *
 * The API must outlive every owner. GPU work and child cleanup precede final release.
 *
 * @param device Acquired device handle.
 * @param parent Instance from which the device was acquired.
 * @param api Command implementation alive through cleanup.
 * @return Shared device owner.
 */
template <class TApi>
[[nodiscard]] auto make_device_ptr(VkDevice device, instance_ptr parent, TApi & api) -> device_ptr
{
    return device_ptr{
        device,
        [parent = std::move(parent), &api](VkDevice handle) mutable noexcept
        {
            api.vk_destroy_device(handle, nullptr);
            // Release the parent now; weak device owners may retain this deleter's control block.
            parent.reset();
        }};
}
}  // namespace vulkandemo::vulkan
