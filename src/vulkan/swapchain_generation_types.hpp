// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_generation_types.hpp
 *
 * Monotonic generation counter for swapchain target lifetimes.
 */
#pragma once

#include <cstdint>

#include <strong_type/incrementable.hpp>
#include <strong_type/ordered.hpp>
#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

namespace vulkandemo::vulkan
{
struct SwapchainGenerationTag;
/** Monotonically increasing identifier distinguishing live swapchain target generations. */
using swapchain_generation = strong::type<
    std::uint64_t,
    SwapchainGenerationTag,
    strong::regular,
    strong::strongly_ordered,
    strong::incrementable>;
}  // namespace vulkandemo::vulkan
