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
#include <memory>
#include <variant>

#include <doctest/doctest.h>

struct VkInstance_T
{
};
struct VkPhysicalDevice_T
{
};
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan::detail
{
auto select_presentation_queues_from_support(
    immer::array<VkQueueFamilyProperties> const & families,
    immer::array<vulkan_result<bool>> const & support)
    -> vulkan_result<std::optional<PresentationQueueFamilies>> pre(
        families.size() == support.size())
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<std::optional<PresentationQueueFamilies>>;
    if (std::ranges::any_of(
            support, [](auto const & outcome) { return result::is_stopped(outcome); }))
    {
        return result::stopped<result_type>();
    }
    auto flags = immer::array<bool>{}.transient();
    for (vulkan_result<bool> const & outcome : support)
    {
        if (result::is_error(outcome))
        {
            return result::error<result_type>(result::get_error(outcome));
        }
        flags.push_back(result::get_success(outcome));
    }
    return result::success<result_type>(
        select_presentation_queue_families(families, std::move(flags).persistent()));
}

auto sequence_presentation_device_query_results(
    VkPhysicalDeviceProperties properties,
    vulkan_result<immer::array<VkExtensionProperties>> extensions,
    vulkan_result<std::optional<PresentationQueueFamilies>> queues)
    -> vulkan_result<presentation_device_query_values>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<presentation_device_query_values>;

    // Any stop takes precedence; otherwise query order determines the first failure.
    if (result::is_stopped(extensions) || result::is_stopped(queues))
    {
        return result::stopped<result_type>();
    }
    if (result::is_error(extensions))
    {
        return result::error<result_type>(result::get_error(std::move(extensions)));
    }
    if (result::is_error(queues))
    {
        return result::error<result_type>(result::get_error(std::move(queues)));
    }

    return result::success<result_type>(
        properties,
        result::get_success(std::move(extensions)),
        result::get_success(std::move(queues)));
}

auto assess_presentation_device_suitability(
    immer::array<text::String> const & names,
    VkPhysicalDeviceProperties const & properties,
    immer::array<VkExtensionProperties> extensions,
    std::optional<PresentationQueueFamilies> queues) -> PresentationDeviceAssessment
{
    auto has_extension = [&](std::string_view name)
    {
        return std::ranges::any_of(
            extensions,
            [&](auto const & extension)
            { return name == std::string_view{extension.extensionName}; });
    };
    bool has_extensions = has_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME) &&
        std::ranges::all_of(names,
                            [&](text::String const & name) { return has_extension(name.view()); });
    return {properties, std::move(queues), has_extensions};
}

auto select_presentation_device_index(
    immer::array<PresentationDeviceAssessment> const & assessments)
    -> std::optional<physical_device_index>
{
    auto properties = immer::array<VkPhysicalDeviceProperties>{}.transient();
    for (PresentationDeviceAssessment const & assessment : assessments)
    {
        properties.push_back(assessment.properties);
    }
    // Retain original indices when unsuitable devices are removed from the scored candidates.
    auto candidates = immer::array<PhysicalDeviceScore>{}.transient();
    for (PhysicalDeviceScore score : score_physical_devices(std::move(properties).persistent()))
    {
        PresentationDeviceAssessment const & assessment = assessments[value_of(score.device_index)];
        if (supports_api_version(api_version{assessment.properties.apiVersion}) &&
            assessment.queue_families && assessment.has_required_extensions)
        {
            candidates.push_back(score);
        }
    }
    return select_physical_device(std::move(candidates).persistent());
}

auto select_presentation_device_from_assessments(
    instance_ptr const & instance,
    surface_ptr const & surface,
    immer::array<VkPhysicalDevice> const & handles,
    immer::array<PresentationDeviceAssessment> const & assessments)
    -> presentation_physical_device_selection_result
    pre(instance != nullptr && surface != nullptr && handles.size() == assessments.size())
{
    namespace result = fnfelt::result;
    using result_type = presentation_physical_device_selection_result;
    std::optional selected_device_index{select_presentation_device_index(assessments)};
    if (!selected_device_index)
    {
        return result::error<result_type>(renderer_error{NoSuitablePhysicalDevice{}});
    }

    std::uint32_t index = value_of(*selected_device_index);
    PresentationDeviceAssessment const & assessment = assessments[index];
    return result::success<result_type>(SelectedPresentationPhysicalDevice{
        instance, surface, handles[index], *assessment.queue_families});
}

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

namespace
{
auto queue(VkQueueFlags flags) -> VkQueueFamilyProperties
{
    VkQueueFamilyProperties properties{};
    properties.queueCount = 1;
    properties.queueFlags = flags;
    return properties;
}

auto extension(char const * name) -> VkExtensionProperties
{
    VkExtensionProperties properties{};
    std::ranges::copy(std::string_view{name}, properties.extensionName);
    return properties;
}

}  // namespace

TEST_CASE("[pure] presentation result sequencing preserves stopped and error outcomes")
{
    namespace result = fnfelt::result;
    VulkanFailure failure{
        FallibleVulkanOperation::get_physical_device_surface_support, VK_ERROR_SURFACE_LOST_KHR};
    immer::array<vulkan_result<bool>> support{
        result::success<vulkan_result<bool>>(false), result::error<vulkan_result<bool>>(failure)};
    SUBCASE("support failure") {}
    SUBCASE("stopped support")
    {
        support = {
            result::success<vulkan_result<bool>>(false), result::stopped<vulkan_result<bool>>()};
    }

    vulkan_result<std::optional<PresentationQueueFamilies>> queues =
        detail::select_presentation_queues_from_support({queue(0), queue(0)}, support);
    vulkan_result<detail::presentation_device_query_values> assessment =
        detail::sequence_presentation_device_query_results(
            {},
            result::success<vulkan_result<immer::array<VkExtensionProperties>>>(
                immer::array<VkExtensionProperties>{}),
            std::move(queues));

    if (result::is_stopped(support[1]))
    {
        CHECK(result::is_stopped(assessment));
    }
    else
    {
        REQUIRE(result::is_error(assessment));
        CHECK_EQ(result::get_error(assessment).result, VK_ERROR_SURFACE_LOST_KHR);
    }
}

TEST_CASE("[pure] presentation support collection prioritises stops over errors")
{
    namespace result = fnfelt::result;
    using support_result = vulkan_result<bool>;
    using queues_result = vulkan_result<std::optional<PresentationQueueFamilies>>;
    VulkanFailure first_failure{
        FallibleVulkanOperation::get_physical_device_surface_support, VK_ERROR_SURFACE_LOST_KHR};
    VulkanFailure later_failure{
        FallibleVulkanOperation::get_physical_device_surface_support, VK_ERROR_OUT_OF_HOST_MEMORY};
    support_result failed = result::error<support_result>(first_failure);
    support_result stopped = result::stopped<support_result>();
    immer::array<support_result> support{failed, stopped};
    bool has_stopped = true;
    bool has_failed = true;
    SUBCASE("error before stop") {}
    SUBCASE("stop before error")
    {
        support = {stopped, failed};
    }
    SUBCASE("first error without a stop")
    {
        support = {failed, result::error<support_result>(later_failure)};
        has_stopped = false;
    }
    SUBCASE("empty support snapshot")
    {
        support = {};
        has_stopped = false;
        has_failed = false;
    }
    immer::array<VkQueueFamilyProperties> families = support.empty()
        ? immer::array<VkQueueFamilyProperties>{}
        : immer::array<VkQueueFamilyProperties>{queue(0), queue(0)};

    queues_result collected = detail::select_presentation_queues_from_support(families, support);

    if (has_stopped)
    {
        CHECK(result::is_stopped(collected));
    }
    else if (has_failed)
    {
        REQUIRE(result::is_error(collected));
        CHECK_EQ(result::get_error(collected).operation, first_failure.operation);
        CHECK_EQ(result::get_error(collected).result, first_failure.result);
    }
    else
    {
        REQUIRE(result::is_success(collected));
        CHECK_FALSE(result::get_success(collected).has_value());
    }
}

TEST_CASE("[pure] presentation assessment collection prioritises stops over errors")
{
    namespace result = fnfelt::result;
    using detail::presentation_device_assessment_result;
    VulkanFailure first_failure{
        FallibleVulkanOperation::enumerate_device_extensions, VK_ERROR_OUT_OF_HOST_MEMORY};
    VulkanFailure later_failure{
        FallibleVulkanOperation::get_physical_device_surface_support, VK_ERROR_SURFACE_LOST_KHR};
    presentation_device_assessment_result failed =
        result::error<presentation_device_assessment_result>(first_failure);
    presentation_device_assessment_result stopped =
        result::stopped<presentation_device_assessment_result>();
    immer::array<presentation_device_assessment_result> assessments{failed, stopped};
    bool has_stopped = true;
    SUBCASE("error before stop") {}
    SUBCASE("stop before error")
    {
        assessments = {stopped, failed};
    }
    SUBCASE("first error without a stop")
    {
        assessments = {failed, result::error<presentation_device_assessment_result>(later_failure)};
        has_stopped = false;
    }
    // A successful prefix does not alter which subsequent failure is propagated.
    presentation_device_assessment_result prefix =
        result::success<presentation_device_assessment_result>(
            detail::PresentationDeviceAssessment{});
    assessments = {prefix, assessments[0], assessments[1]};

    renderer_result<immer::array<detail::PresentationDeviceAssessment>> collected =
        detail::sequence_presentation_device_assessments<detail::PresentationDeviceAssessment>(
            assessments);

    if (has_stopped)
    {
        CHECK(result::is_stopped(collected));
    }
    else
    {
        REQUIRE(result::is_error(collected));
        VulkanOperationFailure const & failure =
            std::get<VulkanOperationFailure>(result::get_error(collected));
        CHECK_EQ(failure.cause.operation, first_failure.operation);
        CHECK_EQ(failure.cause.result, first_failure.result);
    }
}

TEST_CASE("[pure] presentation assessment sequencing extracts values in enumeration order")
{
    namespace result = fnfelt::result;
    using detail::presentation_device_assessment_result;
    VkPhysicalDeviceProperties first_properties{}, second_properties{};
    first_properties.vendorID = 11;
    second_properties.vendorID = 22;
    PresentationQueueFamilies queues{queue_family_index{3}, queue_family_index{7}};
    immer::array<presentation_device_assessment_result> assessments{
        result::success<presentation_device_assessment_result>(
            detail::PresentationDeviceAssessment{first_properties, queues, false}),
        result::success<presentation_device_assessment_result>(
            detail::PresentationDeviceAssessment{second_properties, std::nullopt, true})};
    bool is_empty = false;
    SUBCASE("complete successful snapshot") {}
    SUBCASE("empty successful snapshot")
    {
        assessments = {};
        is_empty = true;
    }

    renderer_result<immer::array<detail::PresentationDeviceAssessment>> outcome =
        detail::sequence_presentation_device_assessments<detail::PresentationDeviceAssessment>(
            assessments);

    REQUIRE(result::is_success(outcome));
    auto const & values = result::get_success(outcome);
    if (is_empty)
    {
        CHECK(values.empty());
    }
    else
    {
        REQUIRE_EQ(values.size(), 2);
        CHECK_EQ(values[0].properties.vendorID, 11);
        REQUIRE(values[0].queue_families.has_value());
        CHECK_EQ(values[0].queue_families->graphics_family_index, queues.graphics_family_index);
        CHECK_FALSE(values[0].has_required_extensions);
        CHECK_EQ(values[1].properties.vendorID, 22);
        CHECK_FALSE(values[1].queue_families.has_value());
        CHECK(values[1].has_required_extensions);
    }
}

TEST_CASE("[pure] presentation ranking preserves original eligible indices and first minimum")
{
    VkPhysicalDeviceProperties discrete{};
    discrete.apiVersion = VK_API_VERSION_1_1;
    discrete.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    VkPhysicalDeviceProperties integrated = discrete;
    integrated.deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    VkPhysicalDeviceProperties old_version = discrete;
    old_version.apiVersion = VK_API_VERSION_1_0;
    PresentationQueueFamilies queues{queue_family_index{3}, queue_family_index{7}};
    detail::PresentationDeviceAssessment first_eligible{discrete, queues, true};
    std::optional expected_index{physical_device_index{2}};
    SUBCASE("old high-scoring prefix and a tied eligible suffix") {}
    SUBCASE("missing queue families exclude the first minimum")
    {
        first_eligible.queue_families = std::nullopt;
        expected_index = physical_device_index{3};
    }
    SUBCASE("missing required extensions exclude the first minimum")
    {
        first_eligible.has_required_extensions = false;
        expected_index = physical_device_index{3};
    }
    immer::array<detail::PresentationDeviceAssessment> assessments{
        {old_version, queues, true},
        {integrated, queues, true},
        first_eligible,
        {discrete, queues, true}};
    SUBCASE("empty snapshot")
    {
        assessments = {};
        expected_index = std::nullopt;
    }
    SUBCASE("no eligible assessments")
    {
        assessments = {
            {old_version, queues, true}, {discrete, std::nullopt, true}, {discrete, queues, false}};
        expected_index = std::nullopt;
    }

    std::optional selected_index{detail::select_presentation_device_index(assessments)};

    CHECK_EQ(selected_index, expected_index);
}

TEST_CASE("[pure] presentation result sequencing prioritises stops over errors")
{
    namespace result = fnfelt::result;
    using extensions_result = vulkan_result<immer::array<VkExtensionProperties>>;
    using queues_result = vulkan_result<std::optional<PresentationQueueFamilies>>;
    VulkanFailure extension_failure{
        FallibleVulkanOperation::enumerate_device_extensions, VK_ERROR_OUT_OF_HOST_MEMORY};
    VulkanFailure queue_failure{
        FallibleVulkanOperation::get_physical_device_surface_support, VK_ERROR_SURFACE_LOST_KHR};
    extensions_result extensions = result::error<extensions_result>(extension_failure);
    queues_result queues = result::stopped<queues_result>();
    bool has_stopped = true;
    SUBCASE("extension error precedes queue stop") {}
    SUBCASE("extension stop precedes queue error")
    {
        extensions = result::stopped<extensions_result>();
        queues = result::error<queues_result>(queue_failure);
    }
    SUBCASE("extension error precedes queue error")
    {
        queues = result::error<queues_result>(queue_failure);
        has_stopped = false;
    }

    vulkan_result<detail::presentation_device_query_values> collected =
        detail::sequence_presentation_device_query_results(
            {}, std::move(extensions), std::move(queues));

    if (has_stopped)
    {
        CHECK(result::is_stopped(collected));
    }
    else
    {
        REQUIRE(result::is_error(collected));
        CHECK_EQ(result::get_error(collected).operation, extension_failure.operation);
        CHECK_EQ(result::get_error(collected).result, extension_failure.result);
    }
}

TEST_CASE("[pure] presentation assessment uses successful capability snapshots")
{
    PresentationQueueFamilies queues{queue_family_index{1}, queue_family_index{2}};

    detail::PresentationDeviceAssessment assessment =
        detail::assess_presentation_device_suitability(
            {}, {}, {extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)}, queues);

    CHECK(assessment.has_required_extensions);
    REQUIRE(assessment.queue_families.has_value());
    CHECK_EQ(assessment.queue_families->graphics_family_index, queues.graphics_family_index);
    CHECK_EQ(assessment.queue_families->present_family_index, queues.present_family_index);
}

}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
