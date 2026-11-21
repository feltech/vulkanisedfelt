// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_instance_configuration.hpp
 *
 * Deferred discovery of maintenance1 presentation-instance prerequisites.
 */
#pragma once

#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "../sdl/managed_handles_types.hpp"
#include "./instance_extensions.hpp"
#include "./presentation_instance_extensions.hpp"
#include "detail/presentation_instance_configuration.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans discovery of all available maintenance1 instance paths for a Vulkan window.
 *
 * The window must have SDL_WINDOW_VULKAN. Its owner is retained through the main-thread SDL
 * query; the resulting names are independently owned. The environment borrows api
 * through a reference wrapper and supplies a serviced main_context; both must outlive execution.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param window Vulkan-capable window owner.
 * @return ReaderIO providing selected owned names, stop, or renderer error.
 */
template <detail::discover_presentation_instance_configuration_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_discover_presentation_instance_configuration(
    sdl::window_ptr window)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;
    // GCC does not support preconditions on this deduced-return factory.
    contract_assert(window != nullptr);

    // ReaderIO providing platform names while retaining the window through its main-thread query.
    auto required_names =
        detail::reader_io_get_required_presentation_extensions<TEnvironment>(std::move(window));

    // ReaderIO providing globally advertised instance extension properties.
    auto available_extensions =
        reader_io_vk_enumerate_instance_extension_properties<TEnvironment>();

    // ReaderIO providing the two independent extension-query outcomes in platform/Vulkan order.
    auto query_results =
        reader_io::sequence<"presentation_instance_extension_queries"_ss, TEnvironment>(
            std::move(required_names), std::move(available_extensions));

    // ReaderIO providing successful query values, any cancellation, or their original failure.
    auto query_values = reader_io::transform<"sequence_presentation_instance_queries"_ss>(
        std::move(query_results), detail::sequence_presentation_instance_query_results);

    // ReaderIO providing selected names and every available maintenance1 instance family.
    return reader_io::transform<"select_presentation_instance_extensions"_ss>(
        std::move(query_values), select_presentation_instance_extensions);
}
}  // namespace vulkandemo::vulkan
