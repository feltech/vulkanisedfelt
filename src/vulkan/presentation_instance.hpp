// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_instance.hpp
 *
 * Deferred instance creation with maintenance1 presentation prerequisites.
 */
#pragma once

#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "../sdl/managed_handles_types.hpp"
#include "./presentation_instance_configuration.hpp"
#include "detail/presentation_instance.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans an instance with platform requirements and every available maintenance1 instance path.
 *
 * The window must have SDL_WINDOW_VULKAN and is retained through prerequisite discovery.
 * Keep the borrowed API and main context alive through execution, and the API through final
 * instance release. Device-extension and maintenance1 feature enablement remain separate steps.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param window Vulkan-capable window owner.
 * @return Reusable ReaderIO providing an instance and its enabled names, stop, or renderer error.
 */
template <detail::create_presentation_instance_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_create_presentation_instance(sdl::window_ptr window)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing owned platform and maintenance1 prerequisite names.
    auto extensions =
        reader_io_discover_presentation_instance_configuration<TEnvironment>(std::move(window));

    // ReaderIO providing an instance only after all required names are available.
    return reader_io::and_then<"create_presentation_instance"_ss>(
        std::move(extensions),
        detail::reader_io_create_configured_presentation_instance<TEnvironment>);
}
}  // namespace vulkandemo::vulkan
