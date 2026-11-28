// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/swapchain_recreation.hpp
 *
 * Stateful swapchain retirement, framebuffer replacement and generation validation.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/state_io.hpp>
#include <immer/box.hpp>

#include "../../sdl/managed_handles_types.hpp"
#include "../managed_handles_types.hpp"
#include "../presentation_device_types.hpp"
#include "../presentation_framebuffers_types.hpp"
#include "../presentation_swapchain_types.hpp"
#include "../renderer_errors.hpp"
#include "../swapchain_framebuffers_types.hpp"
#include "../swapchain_generation_types.hpp"
#include "../swapchain_recreation_types.hpp"
#include "./presentation_swapchain.hpp"
#include "./swapchain_framebuffers.hpp"

namespace vulkandemo::vulkan::detail
{
/** Persistent boxed resource state holding swapchain framebuffer generations. */
template <class TState>
concept swapchain_framebuffers_state = std::same_as<TState, std::remove_cvref_t<TState>> &&
    std::copyable<TState> && requires(TState const & state, SwapchainFramebuffersState next)
{
    typename decltype(state.resources)::value_type;
    typename decltype(state.resources)::memory_policy;
    requires std::same_as<
        decltype(state.resources),
        immer::box<
            typename decltype(state.resources)::value_type,
            typename decltype(state.resources)::memory_policy>>;
    requires std::copyable<typename decltype(state.resources)::value_type>;
    {state.resources.get().swapchain_framebuffers}
        ->std::same_as<SwapchainFramebuffersState const &>;
    requires requires(
        TState mutable_state, typename decltype(state.resources)::value_type mutable_resources)
    {
        mutable_resources.swapchain_framebuffers = std::move(next);
        mutable_state.resources = state.resources.update(std::identity{});
    };
};

/** Execution state and commands needed to recreate presentation framebuffers from boxed inputs. */
template <class TState>
concept recreate_presentation_framebuffers_state = swapchain_framebuffers_state<TState> &&
    create_swapchain_framebuffers_environment<TState> && requires(TState const & state)
{
    {state.resources.get().presentation_device}->std::same_as<PresentationDevice const &>;
    {state.resources.get().window}->std::same_as<sdl::window_ptr const &>;
};

/** Long-lived presentation device and window owners extracted from persistent state. */
struct RecreationContext
{
    /** Retained device, surface and selected presentation queues. */
    PresentationDevice presentation_device;
    /** Nonempty Vulkan-capable window associated with the device's surface. */
    sdl::window_ptr window;
};

/** Swapchain predecessor, reusable render pass and monotonic counter extracted at retirement. */
struct RetiredSwapchainInputs
{
    /** Long-lived device, surface and queue owners from the persistent state box. */
    PresentationDevice presentation_device;
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
 * @param error Failure reported by swapchain preparation or framebuffer replacement.
 * @return Minimized outcome for unavailable extents, otherwise the unmodified renderer error.
 */
auto classify_minimized_or_error(renderer_error error) -> swapchain_recreation_result;

/**
 * Summarizes newly stored framebuffers by their active generation.
 *
 * @param generation Active generation stored for the framebuffers.
 * @param framebuffers Stored presentation framebuffers (ignored after state publication).
 * @return Recreation outcome carrying @p generation.
 */
auto make_recreated_outcome(
    swapchain_generation generation, PresentationFramebuffers const & framebuffers)
    -> swapchain_recreation_outcome;

/**
 * Resolves live presentation framebuffers only when the active generation matches @p expected.
 *
 * @param framebuffers_state Current persistent swapchain framebuffer state.
 * @param expected Generation required by the caller.
 * @return Live framebuffers for @p expected, or `StaleSwapchainGeneration` otherwise.
 */
auto resolve_live_framebuffers(
    SwapchainFramebuffersState const & framebuffers_state, swapchain_generation expected)
    -> renderer_result<PresentationFramebuffers>;

/**
 * Extracts long-lived device and window handles from boxed state without bumping resource refs.
 *
 * @tparam TState Execution state satisfying `recreate_presentation_framebuffers_state`.
 * @param state Owned execution state.
 * @return Extracted device/window handles paired with unchanged state.
 */
template <class TState>
auto read_recreation_context_io_action(TState state) -> std::pair<RecreationContext, TState> pre(
    state.resources.get().presentation_device.device != nullptr &&
    state.resources.get().presentation_device.surface != nullptr &&
    state.resources.get().window != nullptr)
{
    PresentationDevice presentation_device = state.resources.get().presentation_device;
    sdl::window_ptr window = state.resources.get().window;
    return {RecreationContext{std::move(presentation_device), std::move(window)}, std::move(state)};
}

/**
 * Wraps recreation-context extraction for `state_io::create`.
 *
 * @tparam TState Execution state type.
 * @param state Owned execution state.
 * @return Deferred IO producing long-lived context handles and unchanged state.
 */
template <class TState>
constexpr auto read_recreation_context_state_io_action(TState state)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;

    // Callable copying long-lived device and window handles out without duplicating state boxes.
    auto read_context =
        std::bind_front(read_recreation_context_io_action<TState>, std::move(state));

    // IO supplying device and window owners for swapchain preparation.
    return io::create<"read_recreation_context"_ss>(std::move(read_context));
}

/**
 * Lifts live surface and image-parameter preparation from an extracted context snapshot.
 *
 * @tparam TState Execution state type.
 * @param context Long-lived device, surface and window owners.
 * @return StateIO providing validated swapchain configuration or a renderer error.
 */
template <class TState>
constexpr auto state_io_prepare_swapchain_from_context(RecreationContext context)
{
    using fnfelt::literals::operator""_ss;
    return fnfelt::monad::state_io::lift<"prepare_presentation_swapchain"_ss, TState>(
        reader_io_prepare_presentation_swapchain<TState>(
            context.presentation_device, std::move(context.window)));
}

/**
 * Advances generation, extracts predecessor inputs and clears live framebuffers as one IO action.
 *
 * @tparam TState Execution state satisfying `swapchain_framebuffers_state`.
 * @param state Owned execution state.
 * @return Extracted retirement inputs paired with state whose live framebuffers are empty.
 */
template <class TState>
auto retire_active_swapchain_io_action(TState state) -> std::pair<RetiredSwapchainInputs, TState>
{
    SwapchainFramebuffersState const & current = state.resources.get().swapchain_framebuffers;
    swapchain_generation next_generation{value_of(current.latest_generation) + 1};
    RetiredSwapchainInputs inputs{
        state.resources.get().presentation_device, {}, {}, next_generation};
    if (current.live)
    {
        // Extract the live predecessor and render pass before clearing state so replacement
        // creation can hand off oldSwapchain and reuse a format-matched render pass.
        PresentationFramebuffers const & framebuffers = current.live->framebuffers;
        PresentationSwapchain const & presentation_swapchain =
            framebuffers.presentation_image_views.presentation_swapchain_images
                .presentation_swapchain;
        inputs.old_swapchain = presentation_swapchain.swapchain;
        inputs.existing_render_pass = CompatibleRenderPass{
            presentation_swapchain.configuration.surface_format.format, framebuffers.render_pass};
    }
    // Replace the immutable box so sole-owner prior framebuffers and views destroy on exit
    // before vkCreateSwapchainKHR runs, while inputs.old_swapchain stays alive for handoff.
    using resources_type = typename std::remove_cvref_t<decltype(state.resources)>::value_type;
    state.resources = std::move(state.resources)
                          .update(
                              [next_generation](resources_type resources)
                              {
                                  resources.swapchain_framebuffers.latest_generation =
                                      next_generation;
                                  resources.swapchain_framebuffers.live.reset();
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

    // Callable advancing generation and dropping live framebuffers before replacement begins.
    auto retire = std::bind_front(retire_active_swapchain_io_action<TState>, std::move(state));

    // IO producing extracted handoff inputs and cleared live state.
    return io::create<"retire_active_swapchain"_ss>(std::move(retire));
}

/**
 * Publishes newly constructed framebuffers at their reserved generation in immutable boxed
 * storage.
 *
 * @tparam TState Execution state type.
 * @param generation Monotonic generation assigned before framebuffer creation.
 * @param framebuffers Newly assembled presentation framebuffers.
 * @param state Retired execution state receiving @p framebuffers.
 * @return Updated state owning the live framebuffers at @p generation.
 */
template <class TState>
constexpr auto store_live_presentation_framebuffers(
    swapchain_generation generation, PresentationFramebuffers const & framebuffers, TState state)
    -> TState
{
    using resources_type = typename std::remove_cvref_t<decltype(state.resources)>::value_type;
    state.resources = std::move(state.resources)
                          .update(
                              [generation, &framebuffers](resources_type resources)
                              {
                                  resources.swapchain_framebuffers.live =
                                      LivePresentationFramebuffers{generation, framebuffers};
                                  return resources;
                              });
    return state;
}

/**
 * Builds framebuffers from retired handoff inputs and publishes them to state only on success.
 *
 * @tparam TState Execution state type.
 * @param prepared Validated swapchain configuration and surface capabilities.
 * @param inputs Retained device, predecessor swapchain, reusable pass and reserved generation.
 * @return StateIO yielding recreated generation summary or preserving retired state on failure.
 */
template <class TState>
constexpr auto state_io_build_and_store_framebuffers(
    PreparedSwapchain prepared, RetiredSwapchainInputs inputs)
{
    using fnfelt::literals::operator""_ss;
    namespace state_io = fnfelt::monad::state_io;

    // StateIO building replacement framebuffers while leaving live state cleared until success.
    auto built = state_io::lift<"build_recreated_framebuffers"_ss, TState>(
        reader_io_build_prepared_swapchain_framebuffers<TState>(
            std::move(inputs.presentation_device),
            std::move(inputs.old_swapchain),
            std::move(inputs.existing_render_pass),
            prepared));

    // Publication mutator storing recreated framebuffers only on successful completion.
    auto store_framebuffers =
        std::bind_front(store_live_presentation_framebuffers<TState>, inputs.next_generation);

    // Publication runs only for successful framebuffers; errors and stops retain cleared live
    // state.
    auto stored = std::move(built).template store<"store_recreated_framebuffers"_ss>(
        std::move(store_framebuffers));

    // Mapper summarizing newly stored framebuffers by their assigned generation.
    auto summarize_outcome = std::bind_front(make_recreated_outcome, inputs.next_generation);

    // StateIO replacing the large framebuffers payload with its lightweight generation summary.
    return state_io::transform<"summarize_recreated_framebuffers"_ss>(
        std::move(stored), std::move(summarize_outcome));
}

/**
 * Atomically retires any live swapchain and replaces framebuffers for a validated preparation.
 *
 * @tparam TState Execution state type.
 * @param prepared Validated swapchain configuration and surface capabilities.
 * @return StateIO producing recreated summary or renderer error with retired state preserved.
 */
template <class TState>
constexpr auto state_io_replace_prepared_framebuffers(PreparedSwapchain prepared)
{
    using fnfelt::literals::operator""_ss;
    namespace state_io = fnfelt::monad::state_io;

    // StateIO advancing latest_generation, extracting handoff inputs and clearing live state.
    auto retired = state_io::create<"retire_active_swapchain"_ss, TState>(
        retire_active_swapchain_state_io_action<TState>);

    // Continuation building replacement framebuffers from the retired predecessor inputs.
    auto build_and_store = std::bind_front(state_io_build_and_store_framebuffers<TState>, prepared);

    // StateIO assembling replacement framebuffers and publishing them on success.
    return state_io::and_then<"build_and_store_framebuffers"_ss>(
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
 * Inspects current state for live framebuffers matching @p expected as one IO action.
 *
 * @tparam TState Execution state type.
 * @param expected Required framebuffer generation.
 * @param state Owned execution state.
 * @return Live framebuffers or `StaleSwapchainGeneration`, paired with unchanged state.
 */
template <class TState>
auto require_live_framebuffers_io_action(swapchain_generation expected, TState state)
    -> std::pair<renderer_result<PresentationFramebuffers>, TState>
{
    renderer_result<PresentationFramebuffers> outcome =
        resolve_live_framebuffers(state.resources.get().swapchain_framebuffers, expected);
    return {std::move(outcome), std::move(state)};
}

/**
 * Wraps live-generation lookup for `state_io::create`.
 *
 * @tparam TState Execution state type.
 * @param expected Required framebuffer generation.
 * @param state Owned execution state.
 * @return Deferred IO checking whether @p expected is currently live.
 */
template <class TState>
constexpr auto require_live_framebuffers_state_io_action(
    swapchain_generation expected, TState state)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;

    // Callable verifying that the active swapchain generation matches the expected generation.
    auto check_live =
        std::bind_front(require_live_framebuffers_io_action<TState>, expected, std::move(state));

    // IO providing matching framebuffers or StaleSwapchainGeneration without altering state.
    return io::create<"require_live_framebuffers"_ss>(std::move(check_live));
}
}  // namespace vulkandemo::vulkan::detail
