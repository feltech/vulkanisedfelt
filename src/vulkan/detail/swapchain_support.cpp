// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain_support.hpp"

#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/result.hpp>
#include <immer/array.hpp>

#include "../errors.hpp"
#include "../swapchain_support_types.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>

#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan::detail
{
auto sequence_swapchain_support_results(
    vulkan_result<VkSurfaceCapabilitiesKHR> capabilities,
    vulkan_result<immer::array<VkSurfaceFormatKHR>> formats,
    vulkan_result<immer::array<VkPresentModeKHR>> present_modes)
    -> vulkan_result<swapchain_support_query_values>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<swapchain_support_query_values>;

    // Any stop takes precedence; otherwise query order determines the first failure.
    if (result::is_stopped(capabilities) || result::is_stopped(formats) ||
        result::is_stopped(present_modes))
    {
        return result::stopped<result_type>();
    }
    if (result::is_error(capabilities))
    {
        return result::error<result_type>(result::get_error(std::move(capabilities)));
    }
    if (result::is_error(formats))
    {
        return result::error<result_type>(result::get_error(std::move(formats)));
    }
    if (result::is_error(present_modes))
    {
        return result::error<result_type>(result::get_error(std::move(present_modes)));
    }
    return result::success<result_type>(
        result::get_success(std::move(capabilities)),
        result::get_success(std::move(formats)),
        result::get_success(std::move(present_modes)));
}

auto make_swapchain_support(
    VkSurfaceCapabilitiesKHR capabilities,
    immer::array<VkSurfaceFormatKHR> formats,
    immer::array<VkPresentModeKHR> present_modes) -> SwapchainSupport
{
    return {capabilities, std::move(formats), std::move(present_modes)};
}
}  // namespace vulkandemo::vulkan::detail

#ifndef DOCTEST_CONFIG_DISABLE
namespace vulkandemo::vulkan
{
TEST_CASE("[pure] swapchain support joins successful snapshots without applying policy")
{
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.minImageCount = 2;
    immer::array<VkSurfaceFormatKHR> formats{
        VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    immer::array<VkPresentModeKHR> present_modes{VK_PRESENT_MODE_FIFO_KHR};

    SwapchainSupport support =
        detail::make_swapchain_support(capabilities, std::move(formats), std::move(present_modes));

    CHECK_EQ(support.capabilities.minImageCount, 2U);
    REQUIRE_EQ(support.formats.size(), 1);
    CHECK_EQ(support.formats[0].format, VK_FORMAT_B8G8R8A8_UNORM);
    REQUIRE_EQ(support.present_modes.size(), 1);
    CHECK_EQ(support.present_modes[0], VK_PRESENT_MODE_FIFO_KHR);
}

TEST_CASE("[pure] swapchain result sequencing preserves stops and the first snapshot error")
{
    namespace result = fnfelt::result;
    std::uint32_t first_unsuccessful = 0;
    bool is_stopped = false;
    SUBCASE("capability error") {}
    SUBCASE("capability stop")
    {
        is_stopped = true;
    }
    SUBCASE("format error")
    {
        first_unsuccessful = 1;
    }
    SUBCASE("format stop")
    {
        first_unsuccessful = 1;
        is_stopped = true;
    }
    SUBCASE("present-mode error")
    {
        first_unsuccessful = 2;
    }
    SUBCASE("present-mode stop")
    {
        first_unsuccessful = 2;
        is_stopped = true;
    }
    VulkanFailure const capability_failure{
        FallibleVulkanOperation::get_physical_device_surface_capabilities,
        VK_ERROR_OUT_OF_HOST_MEMORY};
    VulkanFailure const format_failure{
        FallibleVulkanOperation::get_physical_device_surface_formats, VK_ERROR_SURFACE_LOST_KHR};
    VulkanFailure const present_mode_failure{
        FallibleVulkanOperation::get_physical_device_surface_present_modes,
        VK_ERROR_OUT_OF_DEVICE_MEMORY};
    vulkan_result<VkSurfaceCapabilitiesKHR> capabilities =
        result::success<vulkan_result<VkSurfaceCapabilitiesKHR>>(VkSurfaceCapabilitiesKHR{});
    vulkan_result<immer::array<VkSurfaceFormatKHR>> formats =
        result::error<vulkan_result<immer::array<VkSurfaceFormatKHR>>>(format_failure);
    vulkan_result<immer::array<VkPresentModeKHR>> present_modes =
        result::error<vulkan_result<immer::array<VkPresentModeKHR>>>(present_mode_failure);
    if (first_unsuccessful == 0)
    {
        capabilities = is_stopped
            ? result::stopped<vulkan_result<VkSurfaceCapabilitiesKHR>>()
            : result::error<vulkan_result<VkSurfaceCapabilitiesKHR>>(capability_failure);
    }
    else if (first_unsuccessful == 1)
    {
        if (is_stopped)
        {
            formats = result::stopped<vulkan_result<immer::array<VkSurfaceFormatKHR>>>();
        }
    }
    else
    {
        formats = result::success<vulkan_result<immer::array<VkSurfaceFormatKHR>>>(
            immer::array<VkSurfaceFormatKHR>{});
        if (is_stopped)
        {
            present_modes = result::stopped<vulkan_result<immer::array<VkPresentModeKHR>>>();
        }
    }

    vulkan_result<detail::swapchain_support_query_values> outcome =
        detail::sequence_swapchain_support_results(
            std::move(capabilities), std::move(formats), std::move(present_modes));

    if (is_stopped)
    {
        CHECK(result::is_stopped(outcome));
    }
    else
    {
        REQUIRE(result::is_error(outcome));
        VulkanFailure const & failure = result::get_error(outcome);
        VulkanFailure const expected = first_unsuccessful == 0 ? capability_failure
            : first_unsuccessful == 1                          ? format_failure
                                                               : present_mode_failure;
        CHECK(failure.operation == expected.operation);
        CHECK_EQ(failure.result, expected.result);
    }
}

TEST_CASE("[pure] swapchain result sequencing prioritises stops over errors in every position")
{
    namespace result = fnfelt::result;
    using capabilities_result = vulkan_result<VkSurfaceCapabilitiesKHR>;
    using formats_result = vulkan_result<immer::array<VkSurfaceFormatKHR>>;
    using present_modes_result = vulkan_result<immer::array<VkPresentModeKHR>>;
    capabilities_result capabilities = result::error<capabilities_result>(VulkanFailure{
        FallibleVulkanOperation::get_physical_device_surface_capabilities,
        VK_ERROR_OUT_OF_HOST_MEMORY});
    formats_result formats = result::error<formats_result>(VulkanFailure{
        FallibleVulkanOperation::get_physical_device_surface_formats, VK_ERROR_SURFACE_LOST_KHR});
    present_modes_result present_modes = result::error<present_modes_result>(VulkanFailure{
        FallibleVulkanOperation::get_physical_device_surface_present_modes,
        VK_ERROR_OUT_OF_DEVICE_MEMORY});
    SUBCASE("capabilities stop")
    {
        capabilities = result::stopped<capabilities_result>();
    }
    SUBCASE("formats stop after a capabilities error")
    {
        formats = result::stopped<formats_result>();
    }
    SUBCASE("present modes stop after both earlier errors")
    {
        present_modes = result::stopped<present_modes_result>();
    }

    vulkan_result<detail::swapchain_support_query_values> outcome =
        detail::sequence_swapchain_support_results(
            std::move(capabilities), std::move(formats), std::move(present_modes));

    CHECK(result::is_stopped(outcome));
}

}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
