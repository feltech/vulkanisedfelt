// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/presentation_instance_configuration.hpp
 *
 * Owned platform queries and result normalization for presentation instance configuration.
 */
#pragma once

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/reader_io.hpp>
#include <immer/array.hpp>

#include "../../sdl/detail/instance_extensions.hpp"
#include "../../sdl/errors.hpp"
#include "../../sdl/managed_handles_types.hpp"
#include "../errors.hpp"
#include "../extension_types.hpp"
#include "../renderer_errors.hpp"
#include "./instance_extensions.hpp"

namespace vulkandemo::vulkan::detail
{
/** Capabilities needed for main-thread platform and global Vulkan extension discovery. */
template <class TEnvironment>
concept discover_presentation_instance_configuration_environment =
    sdl::detail::sdl_vulkan_get_instance_extensions_environment<TEnvironment> &&
    vk_enumerate_instance_extension_properties_environment<TEnvironment>;

/** Successful platform requirements and globally available properties. */
using presentation_instance_query_values =
    std::tuple<immer::array<extension_name>, immer::array<VkExtensionProperties>>;

/**
 * Sequences extension queries with cancellation before errors and platform errors first.
 *
 * @param required_names Platform-query outcome.
 * @param available_extensions Global Vulkan-query outcome.
 * @return Successful values, any stop, or the first query failure.
 */
auto sequence_presentation_instance_query_results(
    sdl::sdl_result<immer::array<extension_name>> required_names,
    vulkan_result<immer::array<VkExtensionProperties>> available_extensions)
    -> renderer_result<presentation_instance_query_values>;

/**
 * Provides required names while owning the queried window.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param window Vulkan-capable window owner.
 * @return Owned platform names or the original SDL failure.
 */
template <class TApi>
auto get_required_presentation_extensions(std::reference_wrapper<TApi> api, sdl::window_ptr window)
    -> sdl::sdl_result<immer::array<extension_name>>
{
    // The synchronous getter runs while this callable still owns the window.
    return sdl::detail::get_instance_extensions(api, window.get());
}

/**
 * Provides the owned platform query on the main thread.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param window Vulkan-capable window owner.
 * @param environment Borrowed API and main-thread context.
 * @return Main-thread IO providing owned platform names or an SDL failure.
 */
template <class TEnvironment>
constexpr auto get_required_presentation_extensions_reader_io_action(
    sdl::window_ptr window, TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;

    // Callable providing names with the window retained for the complete native query.
    auto query = std::bind_front(
        get_required_presentation_extensions<api_type>, environment.api, std::move(window));

    // IO providing platform requirements and immediate error capture on the main thread.
    return io::on<"presentation_extensions_on_main"_ss>(
        environment.main_context,
        io::create<"get_required_presentation_extensions"_ss>(std::move(query)));
}

/**
 * Plans an owned-window query of the platform's required instance names.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param window Vulkan-capable window owner.
 * @return ReaderIO providing owned required names or the original SDL failure.
 */
template <sdl::detail::sdl_vulkan_get_instance_extensions_environment TEnvironment>
constexpr auto reader_io_get_required_presentation_extensions(sdl::window_ptr window)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing the owned main-thread query from the borrowed execution capabilities.
    auto query = std::bind_front(
        get_required_presentation_extensions_reader_io_action<TEnvironment>, std::move(window));

    // ReaderIO providing platform names with the window retained by its executed IO.
    return reader_io::create<"presentation_required_extensions"_ss, TEnvironment>(std::move(query));
}
}  // namespace vulkandemo::vulkan::detail
