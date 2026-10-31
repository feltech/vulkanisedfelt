// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell

/**
 * @file vulkandemo.hpp
 *
 * Entry point for the Vulkan rendering demo.
 */

#pragma once

#include "./Logger.hpp"

namespace vulkandemo
{
/**
 * Run the windowed rendering demo until the window closes.
 *
 * @param logger Destination for setup and rendering diagnostics.
 */
void vulkandemo(LoggerPtr const & logger);
}  // namespace vulkandemo
