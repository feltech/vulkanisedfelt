// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/framebuffer.hpp
 *
 * Deferred creation of presentation framebuffers.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "detail/framebuffer.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans creation of a framebuffer with one color attachment.
 *
 * The nonempty parents must share a device. The view must match the render pass's single
 * color attachment and support the nonzero extent with one layer. The extent must fit the
 * device's framebuffer limits. Keep the borrowed API
 * alive through cleanup, finish GPU work and synchronize host access before final release.
 *
 * @param device Retained logical device.
 * @param render_pass Compatible render pass.
 * @param image_view Retained color attachment.
 * @param extent Selected swapchain extent.
 * @return Reusable ReaderIO providing an owned framebuffer or the original Vulkan failure.
 */
template <detail::vk_create_framebuffer_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_create_framebuffer(
    device_ptr device, render_pass_ptr render_pass, image_view_ptr image_view, VkExtent2D extent)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing framebuffer creation with retained native parents.
    auto create_framebuffer = std::bind_front(
        detail::create_framebuffer_reader_io_action<TEnvironment>,
        std::move(device),
        std::move(render_pass),
        std::move(image_view),
        extent);

    // ReaderIO providing an attachment-sized framebuffer with independently retained parents.
    return reader_io::create<"vk_create_framebuffer"_ss, TEnvironment>(
        std::move(create_framebuffer));
}
}  // namespace vulkandemo::vulkan
