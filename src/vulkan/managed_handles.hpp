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
