// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file vulkan/detail/clear_frame.hpp
 *
 * Acquisition and recording for one bounded clear frame.
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/result.hpp>

#include "../clear_commands_types.hpp"
#include "../errors.hpp"
#include "../image_acquisition_types.hpp"
#include "../managed_handles_types.hpp"
#include "../presentation_framebuffers_types.hpp"
#include "../presentation_swapchain_types.hpp"
#include "../synchronization_types.hpp"
#include "./clear_commands.hpp"
#include "./command_buffer.hpp"
#include "./fence.hpp"
#include "./image_acquisition.hpp"
#include "./semaphore.hpp"
#include "./submit_and_present.hpp"

namespace vulkandemo::vulkan::detail
{
/** Presentation targets owned by the frame environment. */
template <class TEnvironment>
concept frame_targets_environment = requires(TEnvironment const & environment)
{
    requires
        std::same_as<std::remove_const_t<decltype(environment.targets)>, PresentationFramebuffers>;
};

/** Capabilities needed to create one frame's synchronization objects. */
template <class TEnvironment>
concept clear_frame_synchronization_environment =
    std::copy_constructible<TEnvironment> && frame_targets_environment<TEnvironment> &&
    vk_create_fence_environment<TEnvironment> && vk_create_semaphore_environment<TEnvironment>;

/** Capabilities needed to acquire and await an image for one frame. */
template <class TEnvironment>
concept acquire_clear_frame_environment = frame_targets_environment<TEnvironment> &&
    vk_acquire_next_image_environment<TEnvironment> && frame_release_environment<TEnvironment>;

/** Capabilities needed to allocate from the frame-owned graphics pool. */
template <class TEnvironment>
concept allocate_clear_frame_buffer_environment = frame_targets_environment<TEnvironment> &&
    vk_allocate_command_buffer_environment<TEnvironment> &&
    requires(TEnvironment const & environment)
{
    requires
        std::same_as<std::remove_const_t<decltype(environment.command_pool)>, command_pool_ptr>;
};

/** Capabilities needed for one bounded acquisition, clear and presentation. */
template <class TEnvironment>
concept clear_and_present_environment = submit_and_present_environment<TEnvironment> &&
    clear_frame_synchronization_environment<TEnvironment> &&
    acquire_clear_frame_environment<TEnvironment> &&
    allocate_clear_frame_buffer_environment<TEnvironment> &&
    record_clear_commands_environment<TEnvironment>;

/**
 * Observes the swapchain corresponding to a set of presentation targets.
 *
 * @param targets Presentation targets owning the swapchain.
 * @return The retained swapchain.
 */
constexpr auto frame_swapchain(PresentationFramebuffers const & targets) noexcept
    -> PresentationSwapchain const &
{
    return targets.presentation_image_views.presentation_swapchain_images.presentation_swapchain;
}

/** Fresh synchronization objects belonging to one frame execution. */
struct FrameSynchronization
{
    /** Host completion of image acquisition signaling. */
    fence_ptr acquisition_fence;
    /** Completion of the clear-command submission. */
    fence_ptr submission_fence;
    /** Release of presentation resources. */
    fence_ptr presentation_fence;
    /** Rendering signal consumed by presentation. */
    semaphore_ptr rendering_semaphore;
};

/** Image-specific owners and synchronization discovered for this frame. */
struct AcquiredClearFrame
{
    /** Framebuffer matching the acquired image index. */
    framebuffer_ptr framebuffer;
    /** Clear render pass compatible with the acquired framebuffer. */
    render_pass_ptr render_pass;
    /** Render area matching the acquired framebuffer. */
    VkExtent2D extent;
    /** Fresh signals for this execution. */
    FrameSynchronization synchronization;
    /** Host-completed acquisition and its owners. */
    AcquiredSwapchainImage acquired_image;
};

/**
 * Sequences synchronization results with cancellation taking precedence over errors.
 *
 * @param acquisition Acquisition-fence outcome.
 * @param submission Submission-fence outcome.
 * @param presentation Presentation-fence outcome.
 * @param rendering Rendering-semaphore outcome.
 * @return Synchronization owners, stopped, or the first creation failure.
 */
auto sequence_frame_synchronization_results(
    vulkan_result<fence_ptr> acquisition,
    vulkan_result<fence_ptr> submission,
    vulkan_result<fence_ptr> presentation,
    vulkan_result<semaphore_ptr> rendering) -> vulkan_result<FrameSynchronization>;

/**
 * Provides fence creation on the device owning the frame's presentation targets.
 *
 * @param environment Command capabilities and owned presentation targets.
 * @return Deferred fence creation.
 */
template <class TEnvironment>
constexpr auto create_clear_frame_fence_reader_io_action(TEnvironment const & environment)
{
    return io_create_fence(
        environment.api, frame_swapchain(environment.targets).presentation_device.device);
}

/**
 * Provides binary semaphore creation on the device owning the frame's presentation targets.
 *
 * @param environment Command capabilities and owned presentation targets.
 * @return Deferred semaphore creation.
 */
template <class TEnvironment>
constexpr auto create_clear_frame_semaphore_reader_io_action(TEnvironment const & environment)
{
    return io_create_semaphore(
        environment.api, frame_swapchain(environment.targets).presentation_device.device);
}

/**
 * Plans synchronization on the device owning the frame's presentation targets.
 *
 * @return ReaderIO providing fresh frame synchronization or a creation failure.
 */
template <clear_frame_synchronization_environment TEnvironment>
constexpr auto reader_io_create_clear_frame_synchronization()
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // ReaderIO providing a fresh unsignaled fence for each frame completion boundary.
    auto fence = reader_io::create<"create_clear_frame_fence"_ss, TEnvironment>(
        create_clear_frame_fence_reader_io_action<TEnvironment>);

    // ReaderIO providing the rendering signal consumed by presentation.
    auto semaphore = reader_io::create<"create_clear_frame_semaphore"_ss, TEnvironment>(
        create_clear_frame_semaphore_reader_io_action<TEnvironment>);

    // ReaderIO providing all four creation outcomes in frame usage order.
    auto signals = reader_io::sequence<"create_clear_frame_signals"_ss, TEnvironment>(
        fence, fence, fence, std::move(semaphore));

    return reader_io::transform<"clear_frame_synchronization"_ss>(
        std::move(signals), sequence_frame_synchronization_results);
}

/**
 * Executes acquisition and host completion as one IO action.
 *
 * @param api Borrowed command implementation.
 * @param targets Presentation targets in swapchain image order.
 * @param timeout Bound for acquisition only.
 * @param synchronization Fresh synchronization objects.
 * @return Completed acquisition with its frame resources or a failure permitting resource release.
 */
template <class TApi>
auto acquire_clear_frame_io_action(
    std::reference_wrapper<TApi> api,
    PresentationFramebuffers targets,
    timeout_nanoseconds timeout,
    FrameSynchronization synchronization)
    -> vulkan_result<AcquiredClearFrame> pre(
        frame_swapchain(targets).presentation_device.device != nullptr)
        pre(frame_swapchain(targets).swapchain != nullptr)
            pre(targets.render_pass != nullptr && !targets.framebuffers.empty())
                pre(synchronization.acquisition_fence != nullptr)
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<AcquiredClearFrame>;
    PresentationSwapchain const & swapchain = frame_swapchain(targets);
    std::uint32_t image_index = 0;
    VkResult acquisition_status = api.get().vk_acquire_next_image_khr(
        swapchain.presentation_device.device.get(),
        swapchain.swapchain.get(),
        value_of(timeout),
        VK_NULL_HANDLE,
        synchronization.acquisition_fence.get(),
        &image_index);
    if (acquisition_status != VK_SUCCESS && acquisition_status != VK_SUBOPTIMAL_KHR)
    {
        // Non-acquisition statuses leave the supplied signal objects unaffected.
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::acquire_next_image, acquisition_status});
    }

    // Host completion makes a later recording failure safe to abandon through ordinary RAII.
    VkResult status = wait_for_frame_release(
        api, swapchain.presentation_device.device.get(), synchronization.acquisition_fence.get());
    if (status == VK_ERROR_DEVICE_LOST)
    {
        return result::error<result_type>(
            VulkanFailure{FallibleVulkanOperation::wait_for_fence, status});
    }
    AcquiredSwapchainImage acquired_image{
        swapchain.swapchain,
        {},
        synchronization.acquisition_fence,
        swapchain_image_index{image_index},
        acquisition_status == VK_SUBOPTIMAL_KHR};
    // The native index identifies a framebuffer in this swapchain's ordered target snapshot.
    contract_assert(image_index < targets.framebuffers.size());
    return result::success<result_type>(AcquiredClearFrame{
        targets.framebuffers[image_index],
        std::move(targets.render_pass),
        swapchain.configuration.extent,
        std::move(synchronization),
        std::move(acquired_image)});
}

/**
 * Provides host-completed frame acquisition from borrowed capabilities.
 *
 * @param timeout Acquisition timeout.
 * @param synchronization Fresh synchronization objects.
 * @param environment Command capabilities and owned presentation targets.
 * @return Deferred host-completed acquisition.
 */
template <acquire_clear_frame_environment TEnvironment>
constexpr auto acquire_clear_frame_reader_io_action(
    timeout_nanoseconds timeout,
    FrameSynchronization synchronization,
    TEnvironment const & environment)
{
    using fnfelt::literals::operator""_ss;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;
    namespace io = fnfelt::monad::io;

    // Callable providing an acquired image whose acquisition fence already permits release.
    auto acquire = std::bind_front(
        acquire_clear_frame_io_action<api_type>,
        environment.api,
        environment.targets,
        timeout,
        std::move(synchronization));

    // IO providing the acquisition and its complete frame-resource keepalives.
    return io::create<"acquire_clear_frame"_ss>(std::move(acquire));
}

/**
 * Plans host-completed image acquisition with frame ownership.
 *
 * @param timeout Acquisition timeout.
 * @param synchronization Fresh synchronization owners.
 * @return ReaderIO providing one acquired frame.
 */
template <acquire_clear_frame_environment TEnvironment>
constexpr auto reader_io_acquire_clear_frame(
    timeout_nanoseconds timeout, FrameSynchronization synchronization)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing a completed acquisition with all frame owners retained.
    auto acquire_frame = std::bind_front(
        acquire_clear_frame_reader_io_action<TEnvironment>, timeout, std::move(synchronization));

    // ReaderIO providing acquisition completion before any recording or submission.
    return reader_io::create<"acquire_clear_frame"_ss, TEnvironment>(std::move(acquire_frame));
}

/**
 * Provides bounded submission of a recording for its acquired image.
 *
 * @param frame Host-completed acquisition and frame-resource owners.
 * @param recording Executable clear recording.
 * @param environment Command capabilities and the presentation device in the targets.
 * @return IO providing a completed presentation or a failure permitting resource release.
 */
template <class TEnvironment>
constexpr auto present_clear_recording_reader_io_action(
    AcquiredClearFrame frame, RecordedClearCommands recording, TEnvironment const & environment)
{
    return io_submit_and_present(
        environment.api,
        frame_swapchain(environment.targets).presentation_device,
        std::move(recording),
        std::move(frame.acquired_image),
        std::move(frame.synchronization.rendering_semaphore),
        std::move(frame.synchronization.submission_fence),
        std::move(frame.synchronization.presentation_fence));
}

/**
 * Plans bounded submission of executable commands for the acquired frame.
 *
 * @param frame Host-completed acquisition and frame synchronization.
 * @param recording Executable clear recording.
 * @return ReaderIO providing completed presentation or a failure permitting resource release.
 */
template <class TEnvironment>
constexpr auto reader_io_present_clear_recording(
    AcquiredClearFrame frame, RecordedClearCommands recording)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;

    // Action providing completion for the acquired image on its presentation device.
    auto present = std::bind_front(
        present_clear_recording_reader_io_action<TEnvironment>,
        std::move(frame),
        std::move(recording));
    return reader_io::create<"present_clear_recording"_ss, TEnvironment>(std::move(present));
}

/**
 * Provides primary-buffer allocation from the frame environment's graphics pool.
 *
 * @param environment Command capabilities, presentation targets and graphics command pool.
 * @return Deferred primary-buffer allocation.
 */
template <class TEnvironment>
constexpr auto allocate_clear_frame_buffer_reader_io_action(TEnvironment const & environment)
{
    return io_allocate_command_buffer(
        environment.api,
        frame_swapchain(environment.targets).presentation_device.device,
        environment.command_pool);
}

/**
 * Plans a clear recording followed by bounded presentation of the acquired image.
 *
 * @param clear_color Floating-point clear color.
 * @param frame Host-completed acquisition with its targets.
 * @return ReaderIO providing one completed frame or a failure permitting resource release.
 */
template <clear_and_present_environment TEnvironment>
constexpr auto reader_io_record_and_present_clear_frame(
    VkClearColorValue clear_color, AcquiredClearFrame frame)
{
    using fnfelt::literals::operator""_ss;
    namespace reader_io = fnfelt::monad::reader_io;
    // ReaderIO providing a fresh primary buffer from the frame environment's graphics pool.
    auto allocated_buffer = reader_io::create<"allocate_clear_frame_buffer"_ss, TEnvironment>(
        allocate_clear_frame_buffer_reader_io_action<TEnvironment>);

    // Continuation providing an executable clear for the acquired image's corresponding
    // framebuffer.
    auto record_commands = std::bind_front(
        reader_io_record_allocated_clear_commands<TEnvironment>,
        std::move(frame.framebuffer),
        std::move(frame.render_pass),
        frame.extent,
        clear_color);
    auto recording = reader_io::and_then<"record_clear_frame_commands"_ss>(
        std::move(allocated_buffer), std::move(record_commands));

    // Continuation providing bounded presentation of the acquired frame's recording.
    auto present_recording =
        std::bind_front(reader_io_present_clear_recording<TEnvironment>, std::move(frame));

    // ReaderIO providing completion before any captured recording or image owner can be released.
    return reader_io::and_then<"present_clear_recording"_ss>(
        std::move(recording), std::move(present_recording));
}
}  // namespace vulkandemo::vulkan::detail
