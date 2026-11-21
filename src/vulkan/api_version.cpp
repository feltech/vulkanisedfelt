// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./api_version.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>
#include <type_traits>

#include <doctest/doctest.h>

namespace vulkandemo::vulkan
{
static_assert(!std::is_convertible_v<std::uint32_t, api_version>);
static_assert(!std::is_convertible_v<api_version, std::uint32_t>);

TEST_CASE("[pure] API version compatibility respects variants and major minor boundaries")
{
    CHECK_FALSE(supports_api_version(api_version{VK_API_VERSION_1_0}));
    CHECK_FALSE(supports_api_version(api_version{VK_MAKE_API_VERSION(0, 1, 0, 999)}));
    CHECK(supports_api_version(api_version{VK_API_VERSION_1_1}));
    CHECK(supports_api_version(api_version{VK_MAKE_API_VERSION(0, 1, 1, 300)}));
    CHECK(supports_api_version(api_version{VK_MAKE_API_VERSION(0, 1, 4, 0)}));
    CHECK(supports_api_version(api_version{VK_MAKE_API_VERSION(0, 2, 0, 0)}));
    CHECK_FALSE(supports_api_version(api_version{VK_MAKE_API_VERSION(1, 1, 4, 0)}));
    CHECK(supports_api_version(
        api_version{VK_API_VERSION_1_1}, api_version{VK_MAKE_API_VERSION(0, 1, 1, 999)}));
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
