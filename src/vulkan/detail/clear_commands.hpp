// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/clear_commands.hpp
 *
 * Clear-pass recording with retained render targets.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/reader_io.hpp>

#include "../clear_commands_types.hpp"
#include "../command_buffer.hpp"
#include "../managed_handles_types.hpp"

namespace vulkandemo::vulkan::detail
{
/** Capabilities needed to allocate and record a primary clear pass. */
template <class TEnvironment>
concept record_clear_commands_environment = vk_allocate_command_buffer_environment<TEnvironment> &&
    vk_begin_command_buffer_environment<TEnvironment> &&
    vk_end_command_buffer_environment<TEnvironment> &&
    requires(TEnvironment const & environment,
             VkCommandBuffer buffer,
             VkRenderPassBeginInfo const * info)
{
    {environment.api.get().vk_cmd_begin_render_pass(buffer, info, VK_SUBPASS_CONTENTS_INLINE)}
        ->std::same_as<void>;
    {environment.api.get().vk_cmd_end_render_pass(buffer)}->std::same_as<void>;
};

/**
 * Records the clear pass into an already recording primary buffer.
 *
 * @param api Borrowed command implementation.
 * @param framebuffer Retained compatible render target.
 * @param render_pass Retained clear-and-present render pass.
 * @param extent Nonzero render area fitting the framebuffer.
 * @param clear_color Floating-point color for the single color attachment.
 * @param command_buffer Recording primary buffer outside a render pass.
 * @return Buffer with its clear pass recorded.
 */
template <class TApi>
auto record_clear_render_pass_io_action(
    std::reference_wrapper<TApi> api,
    framebuffer_ptr framebuffer,
    render_pass_ptr render_pass,
    VkExtent2D extent,
    VkClearColorValue clear_color,
    command_buffer_ptr command_buffer) -> command_buffer_ptr
    pre(command_buffer != nullptr && framebuffer != nullptr && render_pass != nullptr)
        pre(extent.width > 0 && extent.height > 0)
{
    VkClearValue clear_value{};
    clear_value.color = clear_color;
    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = render_pass.get();
    info.framebuffer = framebuffer.get();
    info.renderArea.extent = extent;
    info.clearValueCount = 1;
    info.pClearValues = &clear_value;
    api.get().vk_cmd_begin_render_pass(command_buffer.get(), &info, VK_SUBPASS_CONTENTS_INLINE);
    // Attachment load/store and final layout transitions provide the clear without draw commands.
    api.get().vk_cmd_end_render_pass(command_buffer.get());
    return command_buffer;
}

/**
 * Provides clear-pass recording from borrowed execution capabilities.
 *
 * @param framebuffer Retained compatible render target.
 * @param render_pass Retained clear-and-present render pass.
 * @param extent Nonzero render area fitting the framebuffer.
 * @param clear_color Floating-point color for the attachment.
 * @param command_buffer Recording primary buffer.
 * @param environment Borrowed command capabilities.
 * @return Deferred clear-pass recording.
 */
template <class TEnvironment>
constexpr auto record_clear_render_pass_reader_io_action(
    framebuffer_ptr framebuffer,
    render_pass_ptr render_pass,
    VkExtent2D extent,
    VkClearColorValue clear_color,
    command_buffer_ptr command_buffer,
    TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing a clear pass while retaining its buffer and render targets.
    auto record_pass = std::bind_front(
        record_clear_render_pass_io_action<api_type>,
        environment.api,
        std::move(framebuffer),
        std::move(render_pass),
        extent,
        clear_color,
        std::move(command_buffer));

    // IO providing the primary buffer after recording its render pass.
    return io::create<"record_clear_render_pass"_ss>(std::move(record_pass));
}

/**
 * Provides clear-pass recording for a buffer in the recording state.
 *
 * @param framebuffer Retained compatible render target.
 * @param render_pass Retained clear-and-present render pass.
 * @param extent Nonzero render area fitting the framebuffer.
 * @param clear_color Floating-point color for the attachment.
 * @param command_buffer Recording primary buffer.
 * @return ReaderIO providing the buffer with its clear pass recorded.
 */
template <class TEnvironment>
constexpr auto reader_io_record_clear_render_pass(
    framebuffer_ptr framebuffer,
    render_pass_ptr render_pass,
    VkExtent2D extent,
    VkClearColorValue clear_color,
    command_buffer_ptr command_buffer)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing clear-pass recording with all native parents retained.
    auto record_clear_pass = std::bind_front(
        record_clear_render_pass_reader_io_action<TEnvironment>,
        std::move(framebuffer),
        std::move(render_pass),
        extent,
        clear_color,
        std::move(command_buffer));

    // ReaderIO providing the primary buffer after its clear-pass commands.
    return reader_io::create<"record_clear_render_pass"_ss, TEnvironment>(
        std::move(record_clear_pass));
}

/**
 * Retains the resources referenced by an executable clear recording.
 *
 * @param render_pass Compatible recorded render pass.
 * @param framebuffer Recorded render target.
 * @param command_buffer Executable primary buffer.
 * @return Recording with its resource keepalives.
 */
auto make_recorded_clear_commands(
    render_pass_ptr render_pass, framebuffer_ptr framebuffer, command_buffer_ptr command_buffer)
    -> RecordedClearCommands;

/**
 * Plans clear commands into an already allocated primary buffer.
 *
 * @param framebuffer Retained render target.
 * @param render_pass Retained compatible clear-and-present render pass.
 * @param extent Nonzero render area fitting the framebuffer.
 * @param clear_color Floating-point color for the color attachment.
 * @param command_buffer Retained initial-state primary buffer.
 * @return ReaderIO providing executable commands with their retained render targets.
 */
template <class TEnvironment>
constexpr auto reader_io_record_allocated_clear_commands(
    framebuffer_ptr framebuffer,
    render_pass_ptr render_pass,
    VkExtent2D extent,
    VkClearColorValue clear_color,
    command_buffer_ptr command_buffer)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing the fresh buffer in one-time recording state.
    auto recording_buffer =
        reader_io_vk_begin_command_buffer<TEnvironment>(std::move(command_buffer));

    // Continuation providing the clear and final-layout transition commands for this target.
    auto record_pass = std::bind_front(
        reader_io_record_clear_render_pass<TEnvironment>,
        framebuffer,
        render_pass,
        extent,
        clear_color);
    auto clear_pass = reader_io::and_then<"record_clear_pass"_ss>(
        std::move(recording_buffer), std::move(record_pass));

    // ReaderIO providing an executable buffer only when native recording completion succeeds.
    auto executable_buffer = reader_io::and_then<"end_clear_commands"_ss>(
        std::move(clear_pass), reader_io_vk_end_command_buffer<TEnvironment>);

    // Callable providing executable commands together with all recorded resource keepalives.
    auto retain_targets = std::bind_front(
        make_recorded_clear_commands, std::move(render_pass), std::move(framebuffer));
    return reader_io::transform<"retain_clear_command_resources"_ss>(
        std::move(executable_buffer), std::move(retain_targets));
}
}  // namespace vulkandemo::vulkan::detail
