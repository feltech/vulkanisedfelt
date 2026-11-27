// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain_recreation.hpp"

#include <utility>
#include <variant>

#include <fnfelt/result.hpp>

#ifndef DOCTEST_CONFIG_DISABLE
#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan::detail
{
auto classify_minimized_or_error(renderer_error error) -> swapchain_recreation_result
{
    namespace result = fnfelt::result;
    if (auto const * config = std::get_if<NoSuitableSwapchainConfiguration>(&error);
        config && config->reason == SwapchainConfigurationFailure::unavailable_extent)
    {
        return result::success<swapchain_recreation_result>(SwapchainMinimized{});
    }
    return result::error<swapchain_recreation_result>(std::move(error));
}

auto make_recreated_outcome(swapchain_generation generation, PresentationFramebuffers const &)
    -> swapchain_recreation_outcome
{
    return SwapchainRecreated{generation};
}

auto resolve_live_targets(
    SwapchainTargetsState const & targets_state, swapchain_generation expected)
    -> renderer_result<PresentationFramebuffers>
{
    namespace result = fnfelt::result;
    using result_type = renderer_result<PresentationFramebuffers>;
    if (targets_state.live && targets_state.live->generation == expected)
    {
        return result::success<result_type>(targets_state.live->targets);
    }
    std::optional<swapchain_generation> active =
        targets_state.live ? std::optional{targets_state.live->generation} : std::nullopt;
    return result::error<result_type>(StaleSwapchainGeneration{expected, active});
}

#ifndef DOCTEST_CONFIG_DISABLE
TEST_CASE("[pure] unavailable extent maps to minimized state while other errors propagate")
{
    namespace result = fnfelt::result;

    swapchain_recreation_result minimized = classify_minimized_or_error(
        NoSuitableSwapchainConfiguration{SwapchainConfigurationFailure::unavailable_extent});
    swapchain_recreation_result bad_format = classify_minimized_or_error(
        NoSuitableSwapchainConfiguration{
            SwapchainConfigurationFailure::no_acceptable_surface_format});
    swapchain_recreation_result vulkan_error = classify_minimized_or_error(
        VulkanOperationFailure{
            VulkanFailure{FallibleVulkanOperation::create_swapchain, VK_ERROR_OUT_OF_DATE_KHR}});

    REQUIRE(result::is_success(minimized));
    CHECK(std::holds_alternative<SwapchainMinimized>(result::get_success(minimized)));
    REQUIRE(result::is_error(bad_format));
    CHECK(
        std::get<NoSuitableSwapchainConfiguration>(result::get_error(bad_format)).reason ==
        SwapchainConfigurationFailure::no_acceptable_surface_format);
    REQUIRE(result::is_error(vulkan_error));
    CHECK_EQ(
        std::get<VulkanOperationFailure>(result::get_error(vulkan_error)).cause.result,
        VK_ERROR_OUT_OF_DATE_KHR);
}

TEST_CASE("[pure] generation lookup enforces matching live targets or reports active generation")
{
    namespace result = fnfelt::result;
    SwapchainTargetsState empty_state{swapchain_generation{2}, std::nullopt};
    SwapchainTargetsState live_state{
        swapchain_generation{3},
        LivePresentationTargets{swapchain_generation{3}, PresentationFramebuffers{}}};

    renderer_result<PresentationFramebuffers> missing =
        resolve_live_targets(empty_state, swapchain_generation{2});
    renderer_result<PresentationFramebuffers> stale =
        resolve_live_targets(live_state, swapchain_generation{1});
    renderer_result<PresentationFramebuffers> matched =
        resolve_live_targets(live_state, swapchain_generation{3});

    REQUIRE(result::is_error(missing));
    CHECK(
        std::get<StaleSwapchainGeneration>(result::get_error(missing)).expected ==
        swapchain_generation{2});
    CHECK_FALSE(std::get<StaleSwapchainGeneration>(result::get_error(missing)).active.has_value());

    REQUIRE(result::is_error(stale));
    CHECK(
        std::get<StaleSwapchainGeneration>(result::get_error(stale)).expected ==
        swapchain_generation{1});
    REQUIRE(std::get<StaleSwapchainGeneration>(result::get_error(stale)).active.has_value());
    CHECK(
        *std::get<StaleSwapchainGeneration>(result::get_error(stale)).active ==
        swapchain_generation{3});

    CHECK(result::is_success(matched));
}
#endif  // DOCTEST_CONFIG_DISABLE
}  // namespace vulkandemo::vulkan::detail
