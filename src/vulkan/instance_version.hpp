// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/instance_version.hpp
 *
 * Deferred discovery of instance-level Vulkan support.
 */
#pragma once

#include <fnfelt/monad/reader_io.hpp>

#include "detail/instance_version.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans instance API version discovery with Vulkan 1.0 fallback.
 *
 * The borrowed API must outlive execution. The reported instance version does not establish
 * support on any particular physical device.
 *
 * @tparam TEnvironment Capabilities supplied when building the action.
 * @return Reusable ReaderIO producing an API version or Vulkan error.
 */
template <detail::vk_enumerate_instance_version_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_enumerate_instance_version()
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing instance API support or the original query failure.
    return reader_io::create<"vk_enumerate_instance_version"_ss, TEnvironment>(
        detail::enumerate_instance_version_reader_io_action<TEnvironment>);
}
}  // namespace vulkandemo::vulkan
