// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/managed_handles.hpp
 *
 * Ordered cleanup of presentation surfaces and their parents.
 */
#pragma once

#include <functional>
#include <memory>

#include <vulkan/vulkan_core.h>

#include "../../execution/main_thread.hpp"
#include "../../sdl/managed_handles_types.hpp"

namespace vulkandemo::vulkan::detail
{
/**
 * Destroys a surface before releasing its window and instance.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param instance Retained instance ownership.
 * @param window Retained window ownership.
 * @param surface Acquired surface handle.
 */
template <class TApi>
void destroy_surface(
    std::reference_wrapper<TApi> api,
    std::shared_ptr<VkInstance_T> & instance,
    sdl::window_ptr & window,
    VkSurfaceKHR surface) noexcept pre(execution::is_main_thread())
{
    api.get().vk_destroy_surface_khr(instance.get(), surface, nullptr);
    // Weak surface observers must not retain either parent after Vulkan cleanup.
    instance.reset();
    window.reset();
}
}  // namespace vulkandemo::vulkan::detail
