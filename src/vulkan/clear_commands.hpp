// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/clear_commands.hpp
 *
 * Deferred one-time clear-pass command recording.
 */
#pragma once

#include <functional>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/reader_io.hpp>

#include "./command_buffer.hpp"
#include "./managed_handles_types.hpp"
#include "detail/clear_commands.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a fresh executable primary buffer containing one color clear pass.
 *
 * The pool must belong to a graphics queue family requested on the device. The framebuffer
 * and clear-and-present render pass must belong to that device, have one single-sample color
 * attachment and cover the nonzero extent. Use floating-point clear values for its UNORM format.
 * Externally serialize host access to the pool and all its buffers, including final releases.
 * Retain the complete recording through all uses and GPU completion; aliases to its buffer alone
 * do not retain the recorded targets. Do not reset or re-record a relied-on recording.
 * Each execution allocates a fresh buffer. Recording neither acquires swapchain images nor
 * submits GPU work; acquisition and submission synchronization are separate requirements.
 *
 * @param device Retained logical device.
 * @param command_pool Retained pool for the graphics queue family.
 * @param framebuffer Retained render target.
 * @param render_pass Retained compatible clear-and-present render pass.
 * @param extent Render area from the selected framebuffer configuration.
 * @param clear_color Floating-point clear color.
 * @return Reusable ReaderIO providing a recording with its resources, or a Vulkan failure.
 */
template <detail::record_clear_commands_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_record_clear_commands(
    device_ptr device,
    command_pool_ptr command_pool,
    framebuffer_ptr framebuffer,
    render_pass_ptr render_pass,
    VkExtent2D extent,
    VkClearColorValue clear_color)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing a fresh primary buffer for every execution of this recording plan.
    auto allocated_buffer = reader_io_vk_allocate_command_buffer<TEnvironment>(
        std::move(device), std::move(command_pool));

    // Continuation providing clear-pass recording and resource retention for the allocated buffer.
    auto record_commands = std::bind_front(
        detail::reader_io_record_allocated_clear_commands<TEnvironment>,
        std::move(framebuffer),
        std::move(render_pass),
        extent,
        clear_color);

    return reader_io::and_then<"record_clear_commands"_ss>(
        std::move(allocated_buffer), std::move(record_commands));
}
}  // namespace vulkandemo::vulkan
