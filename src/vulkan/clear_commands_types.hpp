// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/clear_commands_types.hpp
 *
 * Recorded clear commands with their render target retained.
 */
#pragma once

#include "./errors.hpp"
#include "./managed_handles_types.hpp"

namespace vulkandemo::vulkan
{
/**
 * Executable one-time commands and the resources referenced by their recording.
 *
 * Retain the complete value through all uses, including submitted GPU work and borrowed buffer
 * aliases. Do not reset or re-record its buffer while relying on this recording. Native pool
 * operations and final buffer releases still require externally synchronized host access.
 */
struct RecordedClearCommands
{
    /** Render pass referenced by the recording. */
    render_pass_ptr render_pass;
    /** Framebuffer retaining its attachment and native parents. */
    framebuffer_ptr framebuffer;
    // Reverse member destruction frees the command buffer before releasing recorded resources.
    /** Executable primary buffer, recorded for one submission. */
    command_buffer_ptr command_buffer;
};

/** Outcome of clear-command recording. */
using recorded_clear_commands_result = vulkan_result<RecordedClearCommands>;
}  // namespace vulkandemo::vulkan
