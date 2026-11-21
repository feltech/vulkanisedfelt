// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_framebuffers.hpp"

#include <algorithm>
#include <ranges>
#include <utility>

#include <fnfelt/result.hpp>
#include <immer/array_transient.hpp>

#ifndef DOCTEST_CONFIG_DISABLE
#include <variant>

#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan::detail
{
auto validate_framebuffer_extent(VkExtent2D extent, VkPhysicalDeviceProperties properties)
    -> renderer_result<VkExtent2D> pre(extent.width > 0 && extent.height > 0)
{
    namespace result = fnfelt::result;
    using result_type = renderer_result<VkExtent2D>;
    VkExtent2D maximum{
        properties.limits.maxFramebufferWidth, properties.limits.maxFramebufferHeight};
    if (extent.width > maximum.width || extent.height > maximum.height)
    {
        return result::error<result_type>(UnsupportedFramebufferExtent{extent, maximum});
    }
    return result::success<result_type>(extent);
}

auto sequence_framebuffer_results(immer::array<vulkan_result<framebuffer_ptr>> outcomes)
    -> vulkan_result<immer::array<framebuffer_ptr>>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<immer::array<framebuffer_ptr>>;
    if (std::ranges::any_of(
            outcomes, [](auto const & outcome) { return result::is_stopped(outcome); }))
    {
        return result::stopped<result_type>();
    }
    auto framebuffers = immer::array<framebuffer_ptr>{}.transient();
    for (auto const & outcome : outcomes)
    {
        if (result::is_error(outcome))
        {
            return result::error<result_type>(result::get_error(outcome));
        }
        framebuffers.push_back(result::get_success(outcome));
    }
    return result::success<result_type>(std::move(framebuffers).persistent());
}

auto make_presentation_framebuffers(
    PresentationImageViews snapshot,
    render_pass_ptr render_pass,
    immer::array<framebuffer_ptr> framebuffers) -> PresentationFramebuffers
{
    return {std::move(snapshot), std::move(render_pass), std::move(framebuffers)};
}

#ifndef DOCTEST_CONFIG_DISABLE
TEST_CASE("[pure] framebuffer outcomes prioritize cancellation then the first error")
{
    using outcome_type = vulkan_result<framebuffer_ptr>;
    outcome_type first = fnfelt::result::error<outcome_type>(
        VulkanFailure{FallibleVulkanOperation::create_framebuffer, VK_ERROR_OUT_OF_HOST_MEMORY});
    outcome_type second = fnfelt::result::error<outcome_type>(
        VulkanFailure{FallibleVulkanOperation::create_framebuffer, VK_ERROR_OUT_OF_DEVICE_MEMORY});
    bool has_stop = false;
    SUBCASE("first error follows input order") {}
    SUBCASE("later cancellation overrides an earlier error")
    {
        second = fnfelt::result::stopped<outcome_type>();
        has_stop = true;
    }

    // A stopped child takes precedence over an earlier failed creation.
    vulkan_result<immer::array<framebuffer_ptr>> outcome =
        sequence_framebuffer_results(immer::array{first, second});

    if (has_stop)
    {
        CHECK(fnfelt::result::is_stopped(outcome));
    }
    else
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        CHECK_EQ(fnfelt::result::get_error(outcome).result, VK_ERROR_OUT_OF_HOST_MEMORY);
    }
}
#endif  // DOCTEST_CONFIG_DISABLE
}  // namespace vulkandemo::vulkan::detail
