// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./clear_commands.hpp"

#include <utility>

#include "../clear_commands_types.hpp"
#include "../managed_handles_types.hpp"

namespace vulkandemo::vulkan::detail
{
auto make_recorded_clear_commands(
    render_pass_ptr render_pass, framebuffer_ptr framebuffer, command_buffer_ptr command_buffer)
    -> RecordedClearCommands
{
    return {std::move(render_pass), std::move(framebuffer), std::move(command_buffer)};
}
}  // namespace vulkandemo::vulkan::detail
