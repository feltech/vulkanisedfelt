// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./clear_frame.hpp"

#include <utility>

#include <fnfelt/result.hpp>

#include "../errors.hpp"
#include "../managed_handles_types.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan::detail
{
auto sequence_frame_synchronization_results(
    vulkan_result<fence_ptr> acquisition,
    vulkan_result<fence_ptr> submission,
    vulkan_result<fence_ptr> presentation,
    vulkan_result<semaphore_ptr> rendering) -> vulkan_result<FrameSynchronization>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<FrameSynchronization>;
    if (result::is_stopped(acquisition) || result::is_stopped(submission) ||
        result::is_stopped(presentation) || result::is_stopped(rendering))
    {
        return result::stopped<result_type>();
    }
    if (result::is_error(acquisition))
    {
        return result::error<result_type>(result::get_error(acquisition));
    }
    if (result::is_error(submission))
    {
        return result::error<result_type>(result::get_error(submission));
    }
    if (result::is_error(presentation))
    {
        return result::error<result_type>(result::get_error(presentation));
    }
    if (result::is_error(rendering))
    {
        return result::error<result_type>(result::get_error(rendering));
    }
    return result::success<result_type>(FrameSynchronization{
        result::get_success(std::move(acquisition)),
        result::get_success(std::move(submission)),
        result::get_success(std::move(presentation)),
        result::get_success(std::move(rendering))});
}
}  // namespace vulkandemo::vulkan::detail

#ifndef DOCTEST_CONFIG_DISABLE
namespace vulkandemo::vulkan::detail
{
TEST_CASE("[pure] frame synchronization propagates stops before errors")
{
    namespace result = fnfelt::result;
    VulkanFailure failure{FallibleVulkanOperation::create_fence, VK_ERROR_OUT_OF_HOST_MEMORY};

    vulkan_result<FrameSynchronization> outcome = sequence_frame_synchronization_results(
        result::error<vulkan_result<fence_ptr>>(failure),
        result::success<vulkan_result<fence_ptr>>(fence_ptr{}),
        result::success<vulkan_result<fence_ptr>>(fence_ptr{}),
        result::stopped<vulkan_result<semaphore_ptr>>());

    CHECK(result::is_stopped(outcome));
}
}  // namespace vulkandemo::vulkan::detail
#endif  // DOCTEST_CONFIG_DISABLE
