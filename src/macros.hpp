// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell

/**
 * @file macros.hpp
 *
 * Vulkan result checking helpers.
 */

#pragma once
#include <vulkan/vk_enum_string_helper.h>  // NOLINT(*-include-cleaner)

/**
 * Evaluate a Vulkan operation and throw if its result is not VK_SUCCESS.
 *
 * @param func Expression returning a VkResult.
 * @param msg Description included with the result in the exception message.
 */
#define VK_CHECK(func, msg)                                                                \
    do                                                                                     \
    { /* NOLINT(cppcoreguidelines-avoid-do-while)  */                                      \
        if (const VkResult result = func; result != VK_SUCCESS)                            \
        {                                                                                  \
            throw std::runtime_error{std::format("{}: {}", msg, string_VkResult(result))}; \
        }                                                                                  \
    } while (false)
