// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/extension_types.hpp
 *
 * Owned names identifying Vulkan extensions.
 */
#pragma once

#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

#include "../text/string.hpp"

namespace vulkandemo::vulkan
{
struct ExtensionNameTag;
/** Shared immutable name of a Vulkan extension. */
using extension_name = strong::type<text::String, ExtensionNameTag, strong::regular>;
}  // namespace vulkandemo::vulkan
