// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/api_version_types.hpp
 *
 * Vulkan API versions and the renderer's minimum supported version.
 */
#pragma once

#include <cstdint>

#include <vulkan/vulkan_core.h>

#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

namespace vulkandemo::vulkan
{
struct ApiVersionTag;
/** Encoded Vulkan API version, distinct from implementation-defined driver versions. */
using api_version = strong::type<std::uint32_t, ApiVersionTag, strong::regular>;

/** Minimum standard Vulkan version required by migrated renderer operations. */
inline constexpr api_version minimum_api_version{VK_API_VERSION_1_1};
}  // namespace vulkandemo::vulkan
