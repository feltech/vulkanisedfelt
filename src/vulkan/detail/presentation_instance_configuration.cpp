// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_instance_configuration.hpp"

#include <utility>

#include <fnfelt/result.hpp>

#ifndef DOCTEST_CONFIG_DISABLE
#include <variant>

#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan::detail
{
auto sequence_presentation_instance_query_results(
    sdl::sdl_result<immer::array<extension_name>> required_names,
    vulkan_result<immer::array<VkExtensionProperties>> available_extensions)
    -> renderer_result<presentation_instance_query_values>
{
    namespace result = fnfelt::result;
    using result_type = renderer_result<presentation_instance_query_values>;
    if (result::is_stopped(required_names) || result::is_stopped(available_extensions))
    {
        return result::stopped<result_type>();
    }
    if (result::is_error(required_names))
    {
        return result::error<result_type>(result::get_error(std::move(required_names)));
    }
    if (result::is_error(available_extensions))
    {
        return result::error<result_type>(
            renderer_error_from_vulkan_failure(result::get_error(available_extensions)));
    }
    return result::success<result_type>(
        result::get_success(std::move(required_names)),
        result::get_success(std::move(available_extensions)));
}
}  // namespace vulkandemo::vulkan::detail

#ifndef DOCTEST_CONFIG_DISABLE
namespace vulkandemo::vulkan::detail
{
TEST_CASE("[pure] presentation instance query sequencing prioritizes stops then platform errors")
{
    namespace result = fnfelt::result;
    using required_result = sdl::sdl_result<immer::array<extension_name>>;
    using available_result = vulkan_result<immer::array<VkExtensionProperties>>;
    sdl::SdlFailure sdl_failure{
        sdl::FallibleSdlOperation::enumerate_instance_extensions, {'S', 'D', 'L'}};
    VulkanFailure vulkan_failure{
        FallibleVulkanOperation::enumerate_instance_extensions, VK_ERROR_OUT_OF_HOST_MEMORY};
    bool is_platform_stopped = false;
    bool is_vulkan_stopped = false;
    bool has_sdl_error = true;
    SUBCASE("both queries fail") {}
    SUBCASE("Vulkan fails after platform success")
    {
        has_sdl_error = false;
    }
    SUBCASE("later stop wins over platform failure")
    {
        is_vulkan_stopped = true;
    }
    SUBCASE("platform stop wins over Vulkan failure")
    {
        is_platform_stopped = true;
    }
    required_result required = is_platform_stopped
        ? result::stopped<required_result>()
        : (has_sdl_error ? result::error<required_result>(sdl_failure)
                         : result::success<required_result>(immer::array<extension_name>{}));
    available_result available = is_vulkan_stopped
        ? result::stopped<available_result>()
        : result::error<available_result>(vulkan_failure);

    renderer_result<presentation_instance_query_values> outcome =
        sequence_presentation_instance_query_results(std::move(required), std::move(available));

    if (is_platform_stopped || is_vulkan_stopped)
    {
        CHECK(result::is_stopped(outcome));
    }
    else if (has_sdl_error)
    {
        REQUIRE(result::is_error(outcome));
        sdl::SdlFailure failure = std::get<sdl::SdlFailure>(result::get_error(outcome));
        CHECK_EQ(failure.operation, sdl_failure.operation);
        CHECK_EQ(failure.message, sdl_failure.message);
    }
    else
    {
        REQUIRE(result::is_error(outcome));
        VulkanFailure failure = std::get<VulkanOperationFailure>(result::get_error(outcome)).cause;
        CHECK_EQ(failure.operation, vulkan_failure.operation);
        CHECK_EQ(failure.result, vulkan_failure.result);
    }
}
}  // namespace vulkandemo::vulkan::detail
#endif  // DOCTEST_CONFIG_DISABLE
