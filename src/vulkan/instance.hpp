// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/instance.hpp
 *
 * Deferred creation of a minimal Vulkan instance.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>
#include <immer/array.hpp>

#include "../text/string.hpp"
#include "detail/instance.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a Vulkan 1.0 instance with the requested extensions and default allocation.
 *
 * The environment exposes an api reference_wrapper supporting instance creation and destruction.
 * Its API must remain alive through execution and final instance release.
 *
 * @tparam TEnvironment Capabilities supplied when building the action.
 * @param extension_names Owned extension names.
 * @return Reusable ReaderIO producing an instance owner or Vulkan error.
 */
template <vk_create_instance_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_create_instance(
    immer::array<text::String> extension_names = {})
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Factory producing instance-acquisition IO from an execution environment.
    auto io_create_instance =
        std::bind_front(detail::io_create_instance<TEnvironment>, std::move(extension_names));

    // ReaderIO producing an instance owner or acquisition failure.
    return reader_io::create<"vk_create_instance"_ss, TEnvironment>(std::move(io_create_instance));
}
}  // namespace vulkandemo::vulkan
