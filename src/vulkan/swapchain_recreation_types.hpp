// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/swapchain_recreation_types.hpp
 *
 * State and outcome vocabulary for generation-tracked swapchain recreation.
 */
#pragma once

#include <optional>
#include <variant>

#include "./presentation_framebuffers_types.hpp"
#include "./renderer_errors.hpp"
#include "./swapchain_generation_types.hpp"

namespace vulkandemo::vulkan
{
/** Live presentation targets tagged with the monotonic generation that created them. */
struct LivePresentationTargets
{
    /** Generation counter identifying this live swapchain target set. */
    swapchain_generation generation;
    /** Owned swapchain, views, compatible render pass and framebuffers. */
    PresentationFramebuffers targets;
};

/** Persistent state tracking the active presentation generation and its live targets. */
struct SwapchainTargetsState
{
    /** Highest generation counter allocated so far; zero before any creation attempt. */
    swapchain_generation latest_generation{0};
    /** Currently active presentation targets, or empty when retired or minimized. */
    std::optional<LivePresentationTargets> live;
};

/** Swapchain targets were created or replaced at a new monotonic generation. */
struct SwapchainRecreated
{
    /** Newly active target generation. */
    swapchain_generation generation;
};

/** Drawable surface currently has a zero extent; no swapchain creation occurred. */
struct SwapchainMinimized
{
};

/** Non-error outcome of attempting presentation-target recreation. */
using swapchain_recreation_outcome = std::variant<SwapchainRecreated, SwapchainMinimized>;

/** Complete outcome of presentation-target recreation. */
using swapchain_recreation_result = renderer_result<swapchain_recreation_outcome>;
}  // namespace vulkandemo::vulkan
