// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/presentation_instance.hpp
 *
 * Composition of configured presentation instance ownership.
 */
#pragma once

#include <functional>
#include <utility>

#include <fnfelt/monad/reader_io.hpp>
#include <immer/array.hpp>

#include "../extension_types.hpp"
#include "../instance.hpp"
#include "../managed_handles_types.hpp"
#include "../presentation_instance_types.hpp"
#include "./presentation_instance_configuration.hpp"

namespace vulkandemo::vulkan::detail
{
/** Commands needed to discover prerequisites and acquire a Vulkan instance. */
template <class TEnvironment>
concept create_presentation_instance_environment =
    discover_presentation_instance_configuration_environment<TEnvironment> &&
    vk_create_instance_environment<TEnvironment>;

/**
 * Joins instance ownership with its successfully enabled extension names.
 *
 * @param extension_names Enabled extension names.
 * @param instance Acquired instance owner.
 * @return Instance with its enabled prerequisite facts.
 */
auto make_presentation_instance(immer::array<extension_name> extension_names, instance_ptr instance)
    -> PresentationInstance;

/**
 * Provides an instance with selected prerequisites enabled.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param extension_names Validated extension names.
 * @return ReaderIO providing an instance context or its original acquisition failure.
 */
template <class TEnvironment>
constexpr auto reader_io_create_configured_presentation_instance(
    immer::array<extension_name> extension_names)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing a supported Vulkan instance with all selected extension names.
    auto instance = reader_io_vk_create_instance<TEnvironment>(extension_names);

    // ReaderIO providing ownership alongside the instance families actually enabled.
    return reader_io::transform<"retain_presentation_instance_configuration"_ss>(
        std::move(instance),
        std::bind_front(make_presentation_instance, std::move(extension_names)));
}
}  // namespace vulkandemo::vulkan::detail
