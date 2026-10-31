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

#include "./instance_version.hpp"
#include "./renderer_errors.hpp"
#include "detail/instance.hpp"

namespace vulkandemo::vulkan
{
/**
 * Plans a Vulkan 1.1 instance without layers, extensions or custom allocation.
 *
 * Instance support is checked before creation; unsupported versions return their actual and
 * required API versions. The environment exposes an api reference_wrapper supporting command
 * lookup, instance creation and destruction.
 * Its API must remain alive through execution and final instance release.
 *
 * @tparam TEnvironment Capabilities supplied when building the action.
 * @return Reusable ReaderIO producing an instance owner or renderer error.
 */
template <detail::vk_create_instance_environment TEnvironment>
[[nodiscard]] constexpr auto reader_io_vk_create_instance()
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing the instance implementation's API version.
    auto instance_version = reader_io_vk_enumerate_instance_version<TEnvironment>();

    // ReaderIO providing version support with renderer-domain query failures.
    auto version_with_renderer_errors = reader_io::transform_error<"instance_version_error"_ss>(
        std::move(instance_version), renderer_error_from_vulkan_failure);

    // ReaderIO providing successful baseline validation or a descriptive version error.
    auto supported_version = reader_io::transform<"require_instance_api_version"_ss>(
        std::move(version_with_renderer_errors), detail::require_instance_api_version);

    // Continuation providing native instance creation only after version validation succeeds.
    auto create_instance = detail::reader_io_create_instance<TEnvironment>;

    // ReaderIO providing a supported instance owner or the original validation/acquisition error.
    return reader_io::and_then<"create_supported_instance"_ss>(
        std::move(supported_version), std::move(create_instance));
}
}  // namespace vulkandemo::vulkan
