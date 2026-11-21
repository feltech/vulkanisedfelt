// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/api_version.hpp
 *
 * Compatibility policy for Vulkan API versions.
 */
#pragma once

#include <cstdint>

#include <vulkan/vulkan_core.h>

#include "./api_version_types.hpp"

namespace vulkandemo::vulkan
{
/**
 * Checks API compatibility independently of patch revisions.
 *
 * @param available Version reported by the implementation.
 * @param required Minimum version of the same API variant.
 * @return Whether the variant matches and the major/minor version is sufficient.
 */
constexpr bool supports_api_version(
    api_version available, api_version required = minimum_api_version)
{
    std::uint32_t const available_major = VK_API_VERSION_MAJOR(value_of(available));
    std::uint32_t const required_major = VK_API_VERSION_MAJOR(value_of(required));
    return VK_API_VERSION_VARIANT(value_of(available)) ==
        VK_API_VERSION_VARIANT(value_of(required)) &&
        (available_major > required_major ||
         (available_major == required_major &&
          VK_API_VERSION_MINOR(value_of(available)) >= VK_API_VERSION_MINOR(value_of(required))));
}
}  // namespace vulkandemo::vulkan
