// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/presentation_instance_types.hpp
 *
 * Owned instances with their enabled maintenance1 extension names.
 */
#pragma once

#include <immer/array.hpp>

#include "./extension_types.hpp"
#include "./managed_handles_types.hpp"
#include "./renderer_errors.hpp"

namespace vulkandemo::vulkan
{
/** Instance owner and the extension names successfully enabled at creation. */
struct PresentationInstance
{
    /** Shared Vulkan instance ownership. */
    instance_ptr instance;
    /** Enabled instance names; device support still requires discovery. */
    immer::array<extension_name> enabled_extensions;
};

/** Maintenance1-capable instance acquisition outcome. */
using presentation_instance_result = renderer_result<PresentationInstance>;
}  // namespace vulkandemo::vulkan
