// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_device.hpp"

#include <algorithm>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "../../text/string.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan::detail
{
auto presentation_device_extension_names(immer::array<text::String> required_extension_names)
    -> immer::array<text::String>
{
    auto extension_names = immer::array{text::String{VK_KHR_SWAPCHAIN_EXTENSION_NAME}}.transient();
    for (text::String const & name : required_extension_names)
    {
        if (std::ranges::find(extension_names, name) == extension_names.end())
        {
            extension_names.push_back(name);
        }
    }
    return std::move(extension_names).persistent();
}
}  // namespace vulkandemo::vulkan::detail

#ifndef DOCTEST_CONFIG_DISABLE
namespace vulkandemo::vulkan
{
TEST_CASE("[pure] presentation device extensions contain swapchain and distinct caller names")
{
    immer::array requested{
        text::String{"VK_extra"},
        text::String{VK_KHR_SWAPCHAIN_EXTENSION_NAME},
        text::String{"VK_extra"}};

    immer::array names{detail::presentation_device_extension_names(requested)};

    REQUIRE_EQ(names.size(), 2);
    CHECK_EQ(names[0].view(), VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    CHECK_EQ(names[1].view(), "VK_extra");
    CHECK_EQ(requested.size(), 3);
}

}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
