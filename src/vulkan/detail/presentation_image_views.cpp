// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_image_views.hpp"

#include <algorithm>
#include <ranges>
#include <utility>

#include <fnfelt/result.hpp>
#include <immer/array_transient.hpp>

#ifndef DOCTEST_CONFIG_DISABLE
#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan::detail
{
auto sequence_image_view_results(immer::array<vulkan_result<image_view_ptr>> outcomes)
    -> vulkan_result<immer::array<image_view_ptr>>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<immer::array<image_view_ptr>>;
    if (std::ranges::any_of(
            outcomes, [](auto const & outcome) { return result::is_stopped(outcome); }))
    {
        return result::stopped<result_type>();
    }
    auto views = immer::array<image_view_ptr>{}.transient();
    for (auto const & outcome : outcomes)
    {
        if (result::is_error(outcome))
        {
            return result::error<result_type>(result::get_error(outcome));
        }
        views.push_back(result::get_success(outcome));
    }
    return result::success<result_type>(std::move(views).persistent());
}

auto make_presentation_image_views(
    PresentationSwapchainImages snapshot, immer::array<image_view_ptr> image_views)
    -> PresentationImageViews
{
    return {std::move(snapshot), std::move(image_views)};
}

#ifndef DOCTEST_CONFIG_DISABLE
TEST_CASE("[pure] image view outcomes prioritize cancellation then the first error")
{
    using outcome_type = vulkan_result<image_view_ptr>;
    outcome_type first = fnfelt::result::error<outcome_type>(
        VulkanFailure{FallibleVulkanOperation::create_image_view, VK_ERROR_OUT_OF_HOST_MEMORY});
    outcome_type second = fnfelt::result::error<outcome_type>(
        VulkanFailure{FallibleVulkanOperation::create_image_view, VK_ERROR_OUT_OF_DEVICE_MEMORY});
    bool has_stop = false;
    SUBCASE("first error follows input order") {}
    SUBCASE("later cancellation overrides an earlier error")
    {
        second = fnfelt::result::stopped<outcome_type>();
        has_stop = true;
    }

    // Cancellation is a channel of the combined outcome, never a partial view collection.
    vulkan_result<immer::array<image_view_ptr>> outcome =
        sequence_image_view_results(immer::array{first, second});

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
