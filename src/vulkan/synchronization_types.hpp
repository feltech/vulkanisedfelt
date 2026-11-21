// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/synchronization_types.hpp
 *
 * Time limits for Vulkan synchronization operations.
 */
#pragma once

#include <cstdint>

#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

namespace vulkandemo::vulkan
{
struct TimeoutNanosecondsTag;
/** Native nanosecond timeout; UINT64_MAX requests an unbounded wait where permitted. */
using timeout_nanoseconds = strong::type<std::uint64_t, TimeoutNanosecondsTag, strong::regular>;
}  // namespace vulkandemo::vulkan
