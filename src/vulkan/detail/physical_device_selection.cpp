// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./physical_device_selection.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "../../text/string.hpp"
#include "../api_version.hpp"
#include "../physical_device_selection.hpp"
#include "../physical_device_selection_types.hpp"
#include "../physical_devices_types.hpp"
#include "../queue_family_selection.hpp"
#include "../queue_types.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <variant>

#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan::detail
{
auto sequence_physical_device_query_results(
    VkPhysicalDeviceProperties properties,
    immer::array<VkQueueFamilyProperties> queues,
    vulkan_result<immer::array<VkExtensionProperties>> extensions)
    -> vulkan_result<physical_device_query_values>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<physical_device_query_values>;

    if (result::is_stopped(extensions))
    {
        return result::stopped<result_type>();
    }
    if (result::is_error(extensions))
    {
        return result::error<result_type>(result::get_error(std::move(extensions)));
    }
    return result::success<result_type>(
        properties, std::move(queues), result::get_success(std::move(extensions)));
}

auto assess_physical_device_suitability(
    VkQueueFlags required_flags,
    immer::array<text::String> const & names,
    VkPhysicalDeviceProperties const & properties,
    immer::array<VkQueueFamilyProperties> const & queues,
    immer::array<VkExtensionProperties> extensions) -> PhysicalDeviceAssessment
{
    // Require each complete extension name to appear in the successful query snapshot.
    bool has_extensions = std::ranges::all_of(
        names,
        [&](auto const & name)
        {
            return std::ranges::any_of(
                extensions,
                [&](auto const & available)
                { return name.view() == std::string_view{available.extensionName}; });
        });

    // Score the available queue families and retain the best one meeting the requirements.
    immer::array queue_scores{score_queue_families(queues, required_flags)};
    std::optional selected_queue_family{select_queue_family(queue_scores)};

    // Combine the independent capability checks with the properties used for device ranking.
    return {properties, selected_queue_family, has_extensions};
}

auto select_physical_device_from_assessments(
    instance_ptr const & instance,
    immer::array<VkPhysicalDevice> const & handles,
    immer::array<physical_device_assessment_result> assessments) -> physical_device_selection_result
{
    namespace result = fnfelt::result;

    // All assessments are complete; cancellation takes precedence over query failures.
    if (std::ranges::any_of(
            assessments, [](auto const & assessment) { return result::is_stopped(assessment); }))
    {
        return result::stopped<physical_device_selection_result>();
    }
    auto properties = immer::array<VkPhysicalDeviceProperties>{}.transient();

    // Without cancellation, enumeration order determines the first failure.
    for (auto const & assessment : assessments)
    {
        if (result::is_error(assessment))
        {
            return result::error<physical_device_selection_result>(
                renderer_error_from_vulkan_failure(result::get_error(assessment)));
        }
        properties.push_back(result::get_success(assessment).properties);
    }

    // Filter scores rather than snapshots so indices still address the original handles.
    auto scores = immer::array<PhysicalDeviceScore>{}.transient();
    immer::array device_scores = score_physical_devices(std::move(properties).persistent());
    for (PhysicalDeviceScore score : device_scores)
    {
        PhysicalDeviceAssessment const & assessment =
            result::get_success(assessments[value_of(score.device_index)]);
        if (supports_api_version(api_version{assessment.properties.apiVersion}) &&
            assessment.queue_family_index && assessment.has_required_extensions)
        {
            scores.push_back(score);
        }
    }

    // Choose the first minimum-scored suitable device, or report that none met the requirements.
    std::optional<physical_device_index> selected_device_index =
        select_physical_device(std::move(scores).persistent());
    if (!selected_device_index)
    {
        return result::error<physical_device_selection_result>(
            renderer_error{NoSuitablePhysicalDevice{}});
    }

    // Keep the parent instance alive alongside the selected handle and its assessed queue family.
    PhysicalDeviceAssessment const & selected_assessment =
        result::get_success(assessments[value_of(*selected_device_index)]);
    return result::success<physical_device_selection_result>(SelectedPhysicalDevice{
        instance,
        handles[value_of(*selected_device_index)],
        *selected_assessment.queue_family_index});
}
}  // namespace vulkandemo::vulkan::detail

#ifndef DOCTEST_CONFIG_DISABLE
namespace vulkandemo::vulkan
{
TEST_CASE("[pure] device result sequencing preserves unsuccessful extension queries")
{
    using extensions_result = vulkan_result<immer::array<VkExtensionProperties>>;
    extensions_result extensions = fnfelt::result::stopped<extensions_result>();
    bool has_failed = false;
    SUBCASE("stopped extension query") {}
    SUBCASE("failed extension query")
    {
        extensions = fnfelt::result::error<extensions_result>(VulkanFailure{
            FallibleVulkanOperation::enumerate_device_extensions, VK_ERROR_OUT_OF_HOST_MEMORY});
        has_failed = true;
    }

    vulkan_result<detail::physical_device_query_values> result =
        detail::sequence_physical_device_query_results({}, {}, extensions);

    if (has_failed)
    {
        REQUIRE(fnfelt::result::is_error(result));
        VulkanFailure error = fnfelt::result::get_error(result);
        CHECK(error.operation == FallibleVulkanOperation::enumerate_device_extensions);
        CHECK_EQ(error.result, VK_ERROR_OUT_OF_HOST_MEMORY);
    }
    else
    {
        CHECK(fnfelt::result::is_stopped(result));
    }
}

TEST_CASE("[pure] collected device assessments prioritise stops over errors")
{
    using detail::physical_device_assessment_result;
    physical_device_assessment_result stopped =
        fnfelt::result::stopped<physical_device_assessment_result>();
    physical_device_assessment_result failed =
        fnfelt::result::error<physical_device_assessment_result>(VulkanFailure{
            FallibleVulkanOperation::enumerate_device_extensions, VK_ERROR_OUT_OF_HOST_MEMORY});
    immer::array<physical_device_assessment_result> assessments{stopped, failed};
    bool has_stopped = true;
    SUBCASE("stop before error") {}
    SUBCASE("error before stop")
    {
        assessments = {failed, stopped};
    }
    SUBCASE("first error without a stop")
    {
        physical_device_assessment_result later_failure =
            fnfelt::result::error<physical_device_assessment_result>(VulkanFailure{
                FallibleVulkanOperation::enumerate_device_extensions,
                VK_ERROR_OUT_OF_DEVICE_MEMORY});
        assessments = {failed, later_failure};
        has_stopped = false;
    }
    // A successful prefix must not change stop priority or the original first-error index.
    VkPhysicalDeviceProperties properties{};
    properties.apiVersion = VK_API_VERSION_1_1;
    physical_device_assessment_result prefix =
        fnfelt::result::success<physical_device_assessment_result>(
            detail::PhysicalDeviceAssessment{properties, queue_family_index{0}, true});
    assessments = {prefix, assessments[0], assessments[1]};

    physical_device_selection_result result =
        detail::select_physical_device_from_assessments({}, {}, assessments);

    if (!has_stopped)
    {
        REQUIRE(fnfelt::result::is_error(result));
        VulkanOperationFailure error =
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(result));
        CHECK_EQ(error.cause.result, VK_ERROR_OUT_OF_HOST_MEMORY);
    }
    else
    {
        CHECK(fnfelt::result::is_stopped(result));
    }
}

}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
