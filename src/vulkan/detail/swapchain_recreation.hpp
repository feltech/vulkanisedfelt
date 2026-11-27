// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/swapchain_recreation.hpp
 *
 * Stateful swapchain retirement, target replacement and generation validation.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/state_io.hpp>
#include <immer/box.hpp>

#include "../managed_handles_types.hpp"
#include "../presentation_device_types.hpp"
#include "../presentation_framebuffers_types.hpp"
#include "../presentation_swapchain_types.hpp"
#include "../presentation_targets_types.hpp"
#include "../renderer_errors.hpp"
#include "../swapchain_generation_types.hpp"
#include "../swapchain_recreation_types.hpp"
#include "./presentation_swapchain.hpp"
#include "./presentation_targets.hpp"

namespace vulkandemo::vulkan::detail
{
/** Persistent boxed resource state holding swapchain target generations. */
template <class TState>
concept swapchain_targets_state = std::same_as<TState, std::remove_cvref_t<TState>> &&
    std::copyable<TState> && requires(TState const & state, SwapchainTargetsState next_targets)
{
    typename decltype(state.resources)::value_type;
    typename decltype(state.resources)::memory_policy;
    requires std::same_as<
        decltype(state.resources),
        immer::box<
            typename decltype(state.resources)::value_type,
            typename decltype(state.resources)::memory_policy>>;
    requires std::copyable<typename decltype(state.resources)::value_type>;
    {state.resources.get().swapchain_targets}->std::same_as<SwapchainTargetsState const &>;
    requires requires(
        TState mutable_state, typename decltype(state.resources)::value_type mutable_resources)
    {
        mutable_resources.swapchain_targets = std::move(next_targets);
        mutable_state.resources = state.resources.update(std::identity{});
    };
};

/** Execution state and commands needed to recreate presentation targets cleanly. */
template <class TState>
concept recreate_presentation_targets_state =
    swapchain_targets_state<TState> && create_presentation_targets_environment<TState>;

/** Swapchain predecessor, reusable render pass and monotonic counter extracted at retirement. */
struct RetiredSwapchainInputs
{
    /** Prior swapchain handed off to native replacement, or empty when none is live. */
    swapchain_ptr old_swapchain;
    /** Prior clear-and-present pass eligible for format-matched reuse. */
    CompatibleRenderPass existing_render_pass;
    /** Monotonic generation reserved for the upcoming creation attempt. */
    swapchain_generation next_generation;
};

/**
 * Classifies zero-extent preparation errors as ordinary minimized state while forwarding errors.
 *
 * @param error Failure reported by swapchain preparation or target replacement.
 * @return Minimized outcome for unavailable extents, otherwise the unmodified renderer error.
 */
auto classify_minimized_or_error(renderer_error error) -> swapchain_recreation_result;

/**
 * Summarizes newly stored targets by their active generation.
 *
 * @param generation Active generation stored for the targets.
 * @param targets Stored presentation targets (ignored after state publication).
 * @return Recreation outcome carrying @p generation.
 */
auto make_recreated_outcome(
    swapchain_generation generation, PresentationFramebuffers const & targets)
    -> swapchain_recreation_outcome;

/**
 * Resolves live presentation targets only when the active generation matches @p expected.
 *
 * @param targets_state Current persistent swapchain target state.
 * @param expected Generation required by the caller.
 * @return Live targets for @p expected, or `StaleSwapchainGeneration` otherwise.
 */
auto resolve_live_targets(
    SwapchainTargetsState const & targets_state, swapchain_generation expected)
    -> renderer_result<PresentationFramebuffers>;

/**
 * Advances generation, extracts predecessor inputs and clears live targets as one IO action.
 *
 * @tparam TState Execution state satisfying `swapchain_targets_state`.
 * @param state Owned execution state.
 * @return Extracted retirement inputs paired with state whose live targets are empty.
 */
template <class TState>
auto retire_active_swapchain_io_action(TState state) -> std::pair<RetiredSwapchainInputs, TState>
{
    SwapchainTargetsState const & current = state.resources.get().swapchain_targets;
    swapchain_generation next_generation{value_of(current.latest_generation) + 1};
    RetiredSwapchainInputs inputs{{}, {}, next_generation};
    if (current.live)
    {
        PresentationFramebuffers const & targets = current.live->targets;
        PresentationSwapchain const & presentation_swapchain =
            targets.presentation_image_views.presentation_swapchain_images.presentation_swapchain;
        inputs.old_swapchain = presentation_swapchain.swapchain;
        inputs.existing_render_pass = CompatibleRenderPass{
            presentation_swapchain.configuration.surface_format.format, targets.render_pass};
    }
    // Replace the immutable box so sole-owner prior framebuffers and views destroy on exit
    // before vkCreateSwapchainKHR runs, while inputs.old_swapchain stays alive for handoff.
    using resources_type = typename std::remove_cvref_t<decltype(state.resources)>::value_type;
    state.resources = std::move(state.resources)
                          .update(
                              [next_generation](resources_type resources)
                              {
                                  resources.swapchain_targets.latest_generation = next_generation;
                                  resources.swapchain_targets.live.reset();
                                  return resources;
                              });
    return {std::move(inputs), std::move(state)};
}

/**
 * Wraps swapchain retirement for `state_io::create`.
 *
 * @tparam TState Execution state type.
 * @param state Owned execution state.
 * @return Deferred retirement IO producing extracted inputs and updated state.
 */
template <class TState>
constexpr auto retire_active_swapchain_state_io_action(TState state)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;

    // Callable advancing generation and dropping live targets before replacement begins.
    auto retire = std::bind_front(retire_active_swapchain_io_action<TState>, std::move(state));

    // IO producing extracted handoff inputs and cleared live state.
    return io::create<"retire_active_swapchain"_ss>(std::move(retire));
}

/**
 * Publishes newly constructed targets at their reserved generation in immutable boxed storage.
 *
 * @tparam TState Execution state type.
 * @param generation Monotonic generation assigned before target creation.
 * @param targets Newly assembled presentation targets.
 * @param state Retired execution state receiving @p targets.
 * @return Updated state owning the live targets at @p generation.
 */
template <class TState>
constexpr auto store_live_presentation_targets(
    swapchain_generation generation, PresentationFramebuffers const & targets, TState state)
    -> TState
{
    using resources_type = typename std::remove_cvref_t<decltype(state.resources)>::value_type;
    state.resources = std::move(state.resources)
                          .update(
                              [generation, &targets](resources_type resources)
                              {
                                  resources.swapchain_targets.live =
                                      LivePresentationTargets{generation, targets};
                                  return resources;
                              });
    return state;
}

/**
 * Builds targets from retired handoff inputs and publishes them to state only on success.
 *
 * @tparam TState Execution state type.
 * @param presentation_device Retained device, surface and presentation queues.
 * @param prepared Validated swapchain configuration and surface capabilities.
 * @param inputs Predecessor swapchain, reusable pass and reserved generation.
 * @return StateIO yielding recreated generation summary or preserving retired state on failure.
 */
template <class TState>
constexpr auto state_io_build_and_store_targets(
    PresentationDevice presentation_device,
    PreparedSwapchain prepared,
    RetiredSwapchainInputs inputs)
{
    using fnfelt::literals::operator""_ss;
    namespace state_io = fnfelt::monad::state_io;

    // StateIO building replacement targets while leaving live state cleared until success.
    auto built = state_io::lift<"build_recreated_targets"_ss, TState>(
        reader_io_build_prepared_presentation_targets<TState>(
            std::move(presentation_device),
            std::move(inputs.old_swapchain),
            std::move(inputs.existing_render_pass),
            prepared));

    // Publication mutator storing recreated targets only on successful completion.
    auto store_targets =
        std::bind_front(store_live_presentation_targets<TState>, inputs.next_generation);

    // Publication runs only for successful targets; errors and stops retain cleared live state.
    auto stored =
        std::move(built).template store<"store_recreated_targets"_ss>(std::move(store_targets));

    // Mapper summarizing newly stored targets by their assigned generation.
    auto summarize_outcome = std::bind_front(make_recreated_outcome, inputs.next_generation);

    // StateIO replacing the large targets payload with its lightweight generation summary.
    return state_io::transform<"summarize_recreated_targets"_ss>(
        std::move(stored), std::move(summarize_outcome));
}

/**
 * Atomically retires any live swapchain and replaces targets for a validated preparation.
 *
 * @tparam TState Execution state type.
 * @param presentation_device Retained device, surface and presentation queues.
 * @param prepared Validated swapchain configuration and surface capabilities.
 * @return StateIO producing recreated summary or renderer error with retired state preserved.
 */
template <class TState>
constexpr auto state_io_replace_prepared_targets(
    PresentationDevice presentation_device, PreparedSwapchain prepared)
{
    using fnfelt::literals::operator""_ss;
    namespace state_io = fnfelt::monad::state_io;

    // StateIO advancing latest_generation, extracting handoff inputs and clearing live targets.
    auto retired = state_io::create<"retire_active_swapchain"_ss, TState>(
        retire_active_swapchain_state_io_action<TState>);

    // Continuation building replacement targets from the retired predecessor inputs.
    auto build_and_store = std::bind_front(
        state_io_build_and_store_targets<TState>, std::move(presentation_device), prepared);

    // StateIO assembling replacement targets and publishing them on success.
    return state_io::and_then<"build_and_store_targets"_ss>(
        std::move(retired), std::move(build_and_store));
}

/**
 * Recovers unavailable-extent preparation failures into minimized success while keeping other
 * errors intact.
 *
 * @tparam TState Execution state type.
 * @param error Original preparation or replacement error.
 * @return StateIO holding minimized success or the preserved error with unchanged state.
 */
template <class TState>
constexpr auto state_io_recover_minimized_swapchain(renderer_error error)
{
    using fnfelt::literals::operator""_ss;
    return fnfelt::monad::state_io::just<"handle_minimized_swapchain"_ss, TState>(
        classify_minimized_or_error(std::move(error)));
}

/**
 * Inspects current state for live targets matching @p expected as one IO action.
 *
 * @tparam TState Execution state type.
 * @param expected Required target generation.
 * @param state Owned execution state.
 * @return Live targets or `StaleSwapchainGeneration`, paired with unchanged state.
 */
template <class TState>
auto require_live_targets_io_action(swapchain_generation expected, TState state)
    -> std::pair<renderer_result<PresentationFramebuffers>, TState>
{
    renderer_result<PresentationFramebuffers> outcome =
        resolve_live_targets(state.resources.get().swapchain_targets, expected);
    return {std::move(outcome), std::move(state)};
}

/**
 * Wraps live-generation lookup for `state_io::create`.
 *
 * @tparam TState Execution state type.
 * @param expected Required target generation.
 * @param state Owned execution state.
 * @return Deferred IO checking whether @p expected is currently live.
 */
template <class TState>
constexpr auto require_live_targets_state_io_action(swapchain_generation expected, TState state)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;

    // Callable verifying that the active swapchain generation matches the expected generation.
    auto check_live =
        std::bind_front(require_live_targets_io_action<TState>, expected, std::move(state));

    // IO providing matching targets or StaleSwapchainGeneration without altering state.
    return io::create<"require_live_targets"_ss>(std::move(check_live));
}
}  // namespace vulkandemo::vulkan::detail
