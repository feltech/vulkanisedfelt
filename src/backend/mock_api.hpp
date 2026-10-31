// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file backend/mock_api.hpp
 *
 * Mock Vulkan commands for driverless tests.
 */
#pragma once

#ifndef DOCTEST_CONFIG_DISABLE
#include <vulkan/vulkan_core.h>

#include <trompeloeil.hpp>

namespace vulkandemo::backend
{
/** Per-test Vulkan command expectations. */
struct MockApi
{
    /** Instance creation expectations. */
    MAKE_MOCK3(
        vk_create_instance,
        VkResult(VkInstanceCreateInfo const *, VkAllocationCallbacks const *, VkInstance *));
    /** Instance destruction expectations. */
    MAKE_MOCK2(vk_destroy_instance, void(VkInstance, VkAllocationCallbacks const *));
};
}  // namespace vulkandemo::backend
#endif  // DOCTEST_CONFIG_DISABLE
