// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/instance_layers.hpp
 *
 * Deferred enumeration of global Vulkan instance layers.
 */
#pragma once

#include <utility>

#include <fnfelt/monad/reader_io.hpp>

#include "detail/instance_layers.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans global layer enumeration with at most three count/fill attempts.
 *
 * Incomplete queries and oversized reported counts retry; exhaustion returns VK_INCOMPLETE.
 * The API borrowed through the environment's api reference_wrapper must outlive execution.
 *
 * @tparam TEnvironment Capabilities supplied when building the action.
 * @return Reusable ReaderIO producing immutable properties or Vulkan error.
 */
template <vk_enumerate_instance_layer_properties_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_enumerate_instance_layer_properties()
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory producing instance-layer IO from an execution environment.
    auto io_enumerate_instance_layer_properties =
        detail::io_enumerate_instance_layer_properties<TEnvironment>;

    // ReaderIO producing an owned instance layer snapshot or query failure.
    return reader_io::create<"vk_enumerate_instance_layer_properties"_ss, TEnvironment>(
        std::move(io_enumerate_instance_layer_properties));
}
}  // namespace vulkandemo::vulkan
