// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/render_pass.hpp
 *
 * Deferred creation of clear-and-present render passes.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./managed_handles_types.hpp"
#include "detail/render_pass.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a render pass that clears and stores one presentation attachment.
 *
 * The selected swapchain format must support a single-sample color attachment. Keep the
 * borrowed API alive through cleanup, finish submitted commands using the render pass,
 * and synchronize host access before releasing its final owner.
 *
 * @param device Retained logical device.
 * @param format Selected swapchain image format.
 * @return Reusable ReaderIO providing an owned render pass or the original Vulkan failure.
 */
template <detail::vk_create_render_pass_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_create_render_pass(device_ptr device, VkFormat format)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing render pass creation with its device retained.
    auto create_render_pass = std::bind_front(
        detail::create_render_pass_reader_io_action<TEnvironment>, std::move(device), format);

    // ReaderIO providing one clear-and-present render pass.
    return reader_io::create<"vk_create_render_pass"_ss, TEnvironment>(
        std::move(create_render_pass));
}
}  // namespace vulkandemo::vulkan
