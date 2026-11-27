// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain_recreation.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <SDL_video.h>
#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/monad/state_io.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/box.hpp>
#include <libfork/core/ext/context.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "../sdl/managed_handles_types.hpp"
#include "../sdl/pending_events.hpp"
#include "../sdl/pending_events_types.hpp"
#include "../sdl/video.hpp"
#include "../sdl/vulkan_surface.hpp"
#include "../sdl/window.hpp"
#include "./clear_frame.hpp"
#include "./command_pool.hpp"
#include "./errors.hpp"
#include "./image_acquisition_types.hpp"
#include "./managed_handles_types.hpp"
#include "./presentation_device.hpp"
#include "./presentation_device_types.hpp"
#include "./presentation_framebuffers_types.hpp"
#include "./presentation_instance.hpp"
#include "./presentation_instance_types.hpp"
#include "./queue_types.hpp"
#include "./renderer_errors.hpp"
#include "./submit_and_present_types.hpp"
#include "./swapchain_generation_types.hpp"
#include "./swapchain_recreation_types.hpp"
#include "./synchronization_types.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct RecreationResources
{
    SwapchainTargetsState swapchain_targets;
};

template <class TApi>
struct RecreationState
{
    std::reference_wrapper<TApi> api;
    lf::worker_context * main_context;
    immer::box<RecreationResources> resources;
};

struct MissingRenderPassApi
{
    void vk_destroy_swapchain_khr(VkDevice, VkSwapchainKHR, VkAllocationCallbacks const *);
};

struct NonBoxResourcesState
{
    std::reference_wrapper<backend::Api> api;
    lf::worker_context * main_context;
    RecreationResources resources;
};

struct ReadOnlyResourcesState
{
    std::reference_wrapper<backend::Api> api;
    lf::worker_context * main_context;
    immer::box<RecreationResources> const resources;
};

struct MissingSwapchainTargetsResources
{
    int placeholder{};
};

struct MissingSwapchainTargetsState
{
    std::reference_wrapper<backend::Api> api;
    lf::worker_context * main_context;
    immer::box<MissingSwapchainTargetsResources> resources;
};

struct WrongApiState
{
    std::reference_wrapper<MissingRenderPassApi> api;
    lf::worker_context * main_context;
    immer::box<RecreationResources> resources;
};

static_assert(detail::swapchain_targets_state<RecreationState<backend::Api>>);
static_assert(detail::swapchain_targets_state<RecreationState<backend::MockApi>>);
static_assert(detail::recreate_presentation_targets_state<RecreationState<backend::Api>>);
static_assert(detail::recreate_presentation_targets_state<RecreationState<backend::MockApi>>);

static_assert(!detail::swapchain_targets_state<RecreationState<backend::Api> &>);
static_assert(!detail::swapchain_targets_state<RecreationState<backend::Api> const &>);
static_assert(!detail::swapchain_targets_state<NonBoxResourcesState>);
static_assert(!detail::swapchain_targets_state<ReadOnlyResourcesState>);
static_assert(!detail::swapchain_targets_state<MissingSwapchainTargetsState>);

static_assert(detail::swapchain_targets_state<WrongApiState>);
static_assert(!detail::recreate_presentation_targets_state<WrongApiState>);

template <class TApi>
struct IntegrationFrameEnvironment : sdl::detail::TestState<TApi>
{
    PresentationFramebuffers targets;
    command_pool_ptr command_pool;
};

template <class TApi, class TPlan>
auto run_recreation_state_io(
    sdl::detail::TestRuntime<TApi> & runtime, TPlan const & plan, RecreationState<TApi> & state)
{
    using fnfelt::literals::operator""_ss;
    using base_environment = sdl::detail::TestState<TApi>;
    // Transfer the caller's sole state snapshot so retirement destroys stale targets immediately.
    auto run_recreation = [&](base_environment execution)
    {
        state.api = execution.api;
        state.main_context = execution.main_context;
        auto execution_io = plan.build(std::move(state)).io();
        return fnfelt::monad::io::transform<"unpack_recreation_state_io"_ss>(
            std::move(execution_io),
            [&state](auto outcome_and_state)
            {
                state = std::move(outcome_and_state.second);
                return std::move(outcome_and_state.first);
            });
    };
    return runtime.run(
        fnfelt::monad::reader_io::create<"run_recreation_state_io_test"_ss, base_environment>(
            std::move(run_recreation)));
}

template <class TApi, class TPlan>
auto run_frame_with_targets(
    sdl::detail::TestRuntime<TApi> & runtime,
    TPlan const & plan,
    PresentationFramebuffers targets,
    command_pool_ptr command_pool)
{
    using fnfelt::literals::operator""_ss;
    using base_environment = sdl::detail::TestState<TApi>;
    auto run_frame = [&](base_environment execution)
    {
        return plan
            .build(
                IntegrationFrameEnvironment<TApi>{
                    std::move(execution), std::move(targets), std::move(command_pool)})
            .io();
    };
    return runtime.run(
        fnfelt::monad::reader_io::create<"run_recreated_frame_test"_ss, base_environment>(
            std::move(run_frame)));
}

struct RecreationMockFixture
{
    backend::MockApi api;
    sdl::detail::TestRuntime<backend::MockApi> runtime{api};
    char device_storage{}, physical_storage{}, surface_storage{}, window_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkPhysicalDevice physical = reinterpret_cast<VkPhysicalDevice>(&physical_storage);
    VkSurfaceKHR surface_handle = reinterpret_cast<VkSurfaceKHR>(&surface_storage);
    SDL_Window * window_handle = reinterpret_cast<SDL_Window *>(&window_storage);
    PresentationDevice presentation_device{
        surface_ptr{surface_handle, [](auto) {}},
        device_ptr{device_handle, [](auto) {}},
        physical,
        {queue_family_index{0}, queue_family_index{0}},
        {},
        {}};
    sdl::window_ptr window{window_handle, [](auto) {}};
    RecreationState<backend::MockApi> state{std::ref(api), runtime.environment().main_context, {}};

    auto make_recreate_plan()
    {
        return state_io_recreate_presentation_targets<RecreationState<backend::MockApi>>(
            presentation_device, window);
    }
};

struct RecreationScript
{
    int width = 64;
    int height = 48;
    VkSurfaceFormatKHR format{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkSwapchainKHR expected_old_swapchain = VK_NULL_HANDLE;
    VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
    VkImage new_image = VK_NULL_HANDLE;
    VkImageView new_view = VK_NULL_HANDLE;
    VkRenderPass new_render_pass = VK_NULL_HANDLE;
    VkRenderPass expected_framebuffer_pass = VK_NULL_HANDLE;
    VkFramebuffer new_framebuffer = VK_NULL_HANDLE;
    VkResult support_status = VK_SUCCESS;
    VkResult image_format_status = VK_SUCCESS;
    VkResult create_swapchain_status = VK_SUCCESS;
    VkResult create_framebuffer_status = VK_SUCCESS;
    std::uint32_t max_framebuffer_width = 4096;
    std::uint32_t max_framebuffer_height = 4096;
    bool should_create_render_pass = true;
    std::function<void()> before_create_swapchain{};
    std::function<void()> on_destroy_view{};
    std::function<void()> on_destroy_swapchain{};
    std::function<void()> on_destroy_render_pass{};
    std::function<void()> on_destroy_framebuffer{};
};

void expect_recreation_round(
    backend::MockApi & api,
    RecreationMockFixture const & fixture,
    RecreationScript script,
    std::vector<std::unique_ptr<trompeloeil::expectation>> & expectations)
{
    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api,
            sdl_vulkan_get_drawable_size(fixture.window_handle, trompeloeil::_, trompeloeil::_))
            .SIDE_EFFECT(*_2 = script.width)
            .SIDE_EFFECT(*_3 = script.height));
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.minImageCount = 1;
    capabilities.maxImageCount = 4;
    capabilities.currentExtent = {
        std::numeric_limits<std::uint32_t>::max(), std::numeric_limits<std::uint32_t>::max()};
    capabilities.minImageExtent = {1, 1};
    capabilities.maxImageExtent = {4096, 4096};
    capabilities.maxImageArrayLayers = 1;
    capabilities.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    capabilities.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    capabilities.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    expectations.push_back(NAMED_REQUIRE_CALL(
                               api,
                               vk_get_physical_device_surface_capabilities_khr(
                                   fixture.physical, fixture.surface_handle, trompeloeil::_))
                               .SIDE_EFFECT(*_3 = capabilities)
                               .RETURN(script.support_status));
    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api,
            vk_get_physical_device_surface_formats_khr(
                fixture.physical, fixture.surface_handle, trompeloeil::_, nullptr))
            .SIDE_EFFECT(*_3 = 1)
            .RETURN(VK_SUCCESS));
    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api,
            vk_get_physical_device_surface_formats_khr(
                fixture.physical, fixture.surface_handle, trompeloeil::_, trompeloeil::_))
            .WITH(_4 != nullptr)
            .SIDE_EFFECT(*_4 = script.format)
            .RETURN(VK_SUCCESS));
    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api,
            vk_get_physical_device_surface_present_modes_khr(
                fixture.physical, fixture.surface_handle, trompeloeil::_, nullptr))
            .SIDE_EFFECT(*_3 = 1)
            .RETURN(VK_SUCCESS));
    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api,
            vk_get_physical_device_surface_present_modes_khr(
                fixture.physical, fixture.surface_handle, trompeloeil::_, trompeloeil::_))
            .WITH(_4 != nullptr)
            .SIDE_EFFECT(*_4 = VK_PRESENT_MODE_FIFO_KHR)
            .RETURN(VK_SUCCESS));
    if (script.support_status != VK_SUCCESS || script.width == 0 || script.height == 0)
    {
        return;
    }

    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api,
            vk_get_physical_device_image_format_properties(
                fixture.physical,
                script.format.format,
                VK_IMAGE_TYPE_2D,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                0,
                trompeloeil::_))
            .SIDE_EFFECT(*_7 = VkImageFormatProperties{{4096, 4096, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT, 1048576})
            .RETURN(script.image_format_status));
    if (script.image_format_status != VK_SUCCESS)
    {
        return;
    }

    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api,
            vk_create_swapchain_khr(fixture.device_handle, trompeloeil::_, nullptr, trompeloeil::_))
            .WITH(_2->oldSwapchain == script.expected_old_swapchain)
            .SIDE_EFFECT({
                if (script.before_create_swapchain)
                {
                    script.before_create_swapchain();
                }
                *_4 = script.new_swapchain;
            })
            .RETURN(script.create_swapchain_status));
    if (script.create_swapchain_status != VK_SUCCESS)
    {
        return;
    }

    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api, vk_destroy_swapchain_khr(fixture.device_handle, script.new_swapchain, nullptr))
            .SIDE_EFFECT({
                if (script.on_destroy_swapchain)
                {
                    script.on_destroy_swapchain();
                }
            }));
    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api,
            vk_get_swapchain_images_khr(
                fixture.device_handle, script.new_swapchain, trompeloeil::_, nullptr))
            .SIDE_EFFECT(*_3 = 1)
            .RETURN(VK_SUCCESS));
    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api,
            vk_get_swapchain_images_khr(
                fixture.device_handle, script.new_swapchain, trompeloeil::_, trompeloeil::_))
            .WITH(_4 != nullptr)
            .SIDE_EFFECT(*_4 = script.new_image)
            .RETURN(VK_SUCCESS));
    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api,
            vk_create_image_view(fixture.device_handle, trompeloeil::_, nullptr, trompeloeil::_))
            .WITH(_2->image == script.new_image && _2->format == script.format.format)
            .SIDE_EFFECT(*_4 = script.new_view)
            .RETURN(VK_SUCCESS));
    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api, vk_destroy_image_view(fixture.device_handle, script.new_view, nullptr))
            .SIDE_EFFECT({
                if (script.on_destroy_view)
                {
                    script.on_destroy_view();
                }
            }));

    if (script.should_create_render_pass)
    {
        expectations.push_back(
            NAMED_REQUIRE_CALL(
                api,
                vk_create_render_pass(
                    fixture.device_handle, trompeloeil::_, nullptr, trompeloeil::_))
                .WITH(_2->pAttachments[0].format == script.format.format)
                .SIDE_EFFECT(*_4 = script.new_render_pass)
                .RETURN(VK_SUCCESS));
        expectations.push_back(
            NAMED_REQUIRE_CALL(
                api, vk_destroy_render_pass(fixture.device_handle, script.new_render_pass, nullptr))
                .SIDE_EFFECT({
                    if (script.on_destroy_render_pass)
                    {
                        script.on_destroy_render_pass();
                    }
                }));
    }

    expectations.push_back(
        NAMED_REQUIRE_CALL(api, vk_get_physical_device_properties(fixture.physical, trompeloeil::_))
            .SIDE_EFFECT(_2->limits.maxFramebufferWidth = script.max_framebuffer_width)
            .SIDE_EFFECT(_2->limits.maxFramebufferHeight = script.max_framebuffer_height));
    if (static_cast<std::uint32_t>(script.width) > script.max_framebuffer_width ||
        static_cast<std::uint32_t>(script.height) > script.max_framebuffer_height)
    {
        return;
    }

    expectations.push_back(
        NAMED_REQUIRE_CALL(
            api,
            vk_create_framebuffer(fixture.device_handle, trompeloeil::_, nullptr, trompeloeil::_))
            .WITH(
                _2->renderPass == script.expected_framebuffer_pass &&
                _2->pAttachments[0] == script.new_view)
            .SIDE_EFFECT(*_4 = script.new_framebuffer)
            .RETURN(script.create_framebuffer_status));
    if (script.create_framebuffer_status == VK_SUCCESS)
    {
        expectations.push_back(
            NAMED_REQUIRE_CALL(
                api, vk_destroy_framebuffer(fixture.device_handle, script.new_framebuffer, nullptr))
                .SIDE_EFFECT({
                    if (script.on_destroy_framebuffer)
                    {
                        script.on_destroy_framebuffer();
                    }
                }));
    }
}
}  // namespace

TEST_CASE("[pure] swapchain recreation predicates distinguish recoverable stale states")
{
    namespace result = fnfelt::result;

    CHECK(is_swapchain_out_of_date(
        VulkanFailure{FallibleVulkanOperation::acquire_next_image, VK_ERROR_OUT_OF_DATE_KHR}));
    CHECK(is_swapchain_out_of_date(
        VulkanFailure{FallibleVulkanOperation::queue_present, VK_ERROR_OUT_OF_DATE_KHR}));
    CHECK(is_swapchain_out_of_date(
        VulkanFailure{FallibleVulkanOperation::create_swapchain, VK_ERROR_OUT_OF_DATE_KHR}));
    CHECK_FALSE(is_swapchain_out_of_date(
        VulkanFailure{FallibleVulkanOperation::wait_for_fence, VK_ERROR_OUT_OF_DATE_KHR}));
    CHECK_FALSE(is_swapchain_out_of_date(
        VulkanFailure{FallibleVulkanOperation::acquire_next_image, VK_TIMEOUT}));
    CHECK_FALSE(is_swapchain_out_of_date(
        VulkanFailure{FallibleVulkanOperation::acquire_next_image, VK_NOT_READY}));
    CHECK_FALSE(is_swapchain_out_of_date(
        VulkanFailure{FallibleVulkanOperation::queue_present, VK_ERROR_DEVICE_LOST}));
    CHECK_FALSE(is_swapchain_out_of_date(
        VulkanFailure{FallibleVulkanOperation::queue_present, VK_ERROR_SURFACE_LOST_KHR}));
    CHECK_FALSE(is_swapchain_out_of_date(
        VulkanFailure{FallibleVulkanOperation::create_swapchain, VK_ERROR_OUT_OF_HOST_MEMORY}));

    CHECK_FALSE(should_recreate_swapchain(
        result::success<completed_presentation_result>(
            CompletedPresentation{swapchain_image_index{0}, false})));
    CHECK(should_recreate_swapchain(
        result::success<completed_presentation_result>(
            CompletedPresentation{swapchain_image_index{0}, true})));
    CHECK(should_recreate_swapchain(
        result::error<completed_presentation_result>(
            VulkanFailure{FallibleVulkanOperation::acquire_next_image, VK_ERROR_OUT_OF_DATE_KHR})));
    CHECK(should_recreate_swapchain(
        result::error<completed_presentation_result>(
            VulkanFailure{FallibleVulkanOperation::queue_present, VK_ERROR_OUT_OF_DATE_KHR})));
    CHECK_FALSE(should_recreate_swapchain(
        result::error<completed_presentation_result>(
            VulkanFailure{FallibleVulkanOperation::wait_for_fence, VK_ERROR_DEVICE_LOST})));
    CHECK_FALSE(should_recreate_swapchain(result::stopped<completed_presentation_result>()));
}

TEST_CASE("[impure] swapchain recreation planning is deferred and retains inputs without effects")
{
    RecreationMockFixture fixture;
    std::weak_ptr device_observer{fixture.presentation_device.device};
    std::weak_ptr surface_observer{fixture.presentation_device.surface};
    std::weak_ptr window_observer{fixture.window};

    {
        auto plan = state_io_recreate_presentation_targets<RecreationState<backend::MockApi>>(
            std::move(fixture.presentation_device), std::move(fixture.window));
        [[maybe_unused]] auto discarded = std::move(plan).build(std::move(fixture.state)).io();

        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(surface_observer.expired());
        CHECK_FALSE(window_observer.expired());
    }

    CHECK(device_observer.expired());
    CHECK(surface_observer.expired());
    CHECK(window_observer.expired());
}

TEST_CASE(
    "[impure] reusable swapchain recreation retires old targets before create and reuses "
    "compatible render passes")
{
    RecreationMockFixture fixture;
    char first_swapchain_storage{}, second_swapchain_storage{}, third_swapchain_storage{},
        first_image_storage{}, second_image_storage{}, third_image_storage{}, first_view_storage{},
        second_view_storage{}, third_view_storage{}, first_pass_storage{}, second_pass_storage{},
        first_framebuffer_storage{}, second_framebuffer_storage{}, third_framebuffer_storage{};
    VkSwapchainKHR first_swapchain = reinterpret_cast<VkSwapchainKHR>(&first_swapchain_storage);
    VkSwapchainKHR second_swapchain = reinterpret_cast<VkSwapchainKHR>(&second_swapchain_storage);
    VkSwapchainKHR third_swapchain = reinterpret_cast<VkSwapchainKHR>(&third_swapchain_storage);
    VkRenderPass first_render_pass = reinterpret_cast<VkRenderPass>(&first_pass_storage);
    VkRenderPass second_render_pass = reinterpret_cast<VkRenderPass>(&second_pass_storage);
    VkFramebuffer first_framebuffer = reinterpret_cast<VkFramebuffer>(&first_framebuffer_storage);
    VkFramebuffer second_framebuffer = reinterpret_cast<VkFramebuffer>(&second_framebuffer_storage);
    VkFramebuffer third_framebuffer = reinterpret_cast<VkFramebuffer>(&third_framebuffer_storage);
    std::vector<std::string> events;
    std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
    auto plan = fixture.make_recreate_plan();

    // Run 1: initial creation (gen 0 -> gen 1).
    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .width = 64,
            .height = 48,
            .format = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            .expected_old_swapchain = VK_NULL_HANDLE,
            .new_swapchain = first_swapchain,
            .new_image = reinterpret_cast<VkImage>(&first_image_storage),
            .new_view = reinterpret_cast<VkImageView>(&first_view_storage),
            .new_render_pass = first_render_pass,
            .expected_framebuffer_pass = first_render_pass,
            .new_framebuffer = first_framebuffer,
            .should_create_render_pass = true,
            .on_destroy_view = [&] { events.emplace_back("destroy_view_1"); },
            .on_destroy_swapchain = [&] { events.emplace_back("destroy_swapchain_1"); },
            .on_destroy_render_pass = [&] { events.emplace_back("destroy_pass_1"); },
            .on_destroy_framebuffer = [&] { events.emplace_back("destroy_fb_1"); }},
        expectations);
    swapchain_recreation_result first =
        run_recreation_state_io(fixture.runtime, plan, fixture.state);
    REQUIRE(fnfelt::result::is_success(first));
    CHECK(
        std::get<SwapchainRecreated>(fnfelt::result::get_success(first)).generation ==
        swapchain_generation{1});
    CHECK(
        fixture.state.resources.get().swapchain_targets.latest_generation ==
        swapchain_generation{1});
    REQUIRE(fixture.state.resources.get().swapchain_targets.live.has_value());

    // Run 2: resize with matching format (gen 1 -> gen 2, reuses first_render_pass).
    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .width = 128,
            .height = 96,
            .format = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            .expected_old_swapchain = first_swapchain,
            .new_swapchain = second_swapchain,
            .new_image = reinterpret_cast<VkImage>(&second_image_storage),
            .new_view = reinterpret_cast<VkImageView>(&second_view_storage),
            .new_render_pass = VK_NULL_HANDLE,
            .expected_framebuffer_pass = first_render_pass,
            .new_framebuffer = second_framebuffer,
            .should_create_render_pass = false,
            .before_create_swapchain = [&] { events.emplace_back("create_swapchain_2"); },
            .on_destroy_view = [&] { events.emplace_back("destroy_view_2"); },
            .on_destroy_swapchain = [&] { events.emplace_back("destroy_swapchain_2"); },
            .on_destroy_framebuffer = [&] { events.emplace_back("destroy_fb_2"); }},
        expectations);
    swapchain_recreation_result second =
        run_recreation_state_io(fixture.runtime, plan, fixture.state);
    REQUIRE(fnfelt::result::is_success(second));
    CHECK(
        std::get<SwapchainRecreated>(fnfelt::result::get_success(second)).generation ==
        swapchain_generation{2});
    REQUIRE(fixture.state.resources.get().swapchain_targets.live.has_value());
    CHECK_EQ(
        fixture.state.resources.get().swapchain_targets.live->targets.render_pass.get(),
        first_render_pass);
    REQUIRE_EQ(
        events,
        std::vector<std::string>{
            "destroy_fb_1", "destroy_view_1", "create_swapchain_2", "destroy_swapchain_1"});

    // Run 3: format change (gen 2 -> gen 3, creates second_render_pass and drops first pass after
    // second_framebuffer destroyed).
    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .width = 200,
            .height = 100,
            .format = {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            .expected_old_swapchain = second_swapchain,
            .new_swapchain = third_swapchain,
            .new_image = reinterpret_cast<VkImage>(&third_image_storage),
            .new_view = reinterpret_cast<VkImageView>(&third_view_storage),
            .new_render_pass = second_render_pass,
            .expected_framebuffer_pass = second_render_pass,
            .new_framebuffer = third_framebuffer,
            .should_create_render_pass = true,
            .before_create_swapchain = [&] { events.emplace_back("create_swapchain_3"); }},
        expectations);
    swapchain_recreation_result third =
        run_recreation_state_io(fixture.runtime, plan, fixture.state);
    REQUIRE(fnfelt::result::is_success(third));
    CHECK(
        std::get<SwapchainRecreated>(fnfelt::result::get_success(third)).generation ==
        swapchain_generation{3});
    REQUIRE(fixture.state.resources.get().swapchain_targets.live.has_value());
    CHECK_EQ(
        fixture.state.resources.get().swapchain_targets.live->targets.render_pass.get(),
        second_render_pass);
    REQUIRE_EQ(
        events,
        std::vector<std::string>{
            "destroy_fb_1",
            "destroy_view_1",
            "create_swapchain_2",
            "destroy_swapchain_1",
            "destroy_fb_2",
            "destroy_view_2",
            "create_swapchain_3",
            "destroy_swapchain_2",
            "destroy_pass_1"});

    fixture.state.resources = {};
}

TEST_CASE("[impure] minimized surfaces return SwapchainMinimized without retiring live targets")
{
    RecreationMockFixture fixture;
    char first_swapchain_storage{}, second_swapchain_storage{}, first_image_storage{},
        second_image_storage{}, first_view_storage{}, second_view_storage{}, first_pass_storage{},
        first_framebuffer_storage{}, second_framebuffer_storage{};
    VkSwapchainKHR first_swapchain = reinterpret_cast<VkSwapchainKHR>(&first_swapchain_storage);
    VkSwapchainKHR second_swapchain = reinterpret_cast<VkSwapchainKHR>(&second_swapchain_storage);
    VkRenderPass first_render_pass = reinterpret_cast<VkRenderPass>(&first_pass_storage);
    std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
    auto plan = fixture.make_recreate_plan();

    // Minimized at startup keeps latest_generation at 0 and live empty.
    expect_recreation_round(
        fixture.api, fixture, RecreationScript{.width = 0, .height = 0}, expectations);
    swapchain_recreation_result initial_minimized =
        run_recreation_state_io(fixture.runtime, plan, fixture.state);
    REQUIRE(fnfelt::result::is_success(initial_minimized));
    CHECK(
        std::holds_alternative<SwapchainMinimized>(fnfelt::result::get_success(initial_minimized)));
    CHECK(
        fixture.state.resources.get().swapchain_targets.latest_generation ==
        swapchain_generation{0});
    CHECK_FALSE(fixture.state.resources.get().swapchain_targets.live.has_value());

    // Acquire gen 1 cleanly.
    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .width = 64,
            .height = 48,
            .expected_old_swapchain = VK_NULL_HANDLE,
            .new_swapchain = first_swapchain,
            .new_image = reinterpret_cast<VkImage>(&first_image_storage),
            .new_view = reinterpret_cast<VkImageView>(&first_view_storage),
            .new_render_pass = first_render_pass,
            .expected_framebuffer_pass = first_render_pass,
            .new_framebuffer = reinterpret_cast<VkFramebuffer>(&first_framebuffer_storage)},
        expectations);
    swapchain_recreation_result created =
        run_recreation_state_io(fixture.runtime, plan, fixture.state);
    REQUIRE(fnfelt::result::is_success(created));
    CHECK(
        fixture.state.resources.get().swapchain_targets.latest_generation ==
        swapchain_generation{1});
    REQUIRE(fixture.state.resources.get().swapchain_targets.live.has_value());

    // Minimizing while live preserves gen 1 unretired and avoids calling vkCreateSwapchainKHR.
    expect_recreation_round(
        fixture.api, fixture, RecreationScript{.width = 0, .height = 48}, expectations);
    swapchain_recreation_result minimized_while_live =
        run_recreation_state_io(fixture.runtime, plan, fixture.state);
    REQUIRE(fnfelt::result::is_success(minimized_while_live));
    CHECK(
        std::holds_alternative<SwapchainMinimized>(
            fnfelt::result::get_success(minimized_while_live)));
    CHECK(
        fixture.state.resources.get().swapchain_targets.latest_generation ==
        swapchain_generation{1});
    REQUIRE(fixture.state.resources.get().swapchain_targets.live.has_value());
    CHECK(
        fixture.state.resources.get().swapchain_targets.live->generation ==
        swapchain_generation{1});

    // Un-minimizing hands off the still-live gen 1 swapchain cleanly.
    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .width = 100,
            .height = 80,
            .expected_old_swapchain = first_swapchain,
            .new_swapchain = second_swapchain,
            .new_image = reinterpret_cast<VkImage>(&second_image_storage),
            .new_view = reinterpret_cast<VkImageView>(&second_view_storage),
            .new_render_pass = VK_NULL_HANDLE,
            .expected_framebuffer_pass = first_render_pass,
            .new_framebuffer = reinterpret_cast<VkFramebuffer>(&second_framebuffer_storage),
            .should_create_render_pass = false},
        expectations);
    swapchain_recreation_result unminimized =
        run_recreation_state_io(fixture.runtime, plan, fixture.state);
    REQUIRE(fnfelt::result::is_success(unminimized));
    CHECK(
        std::get<SwapchainRecreated>(fnfelt::result::get_success(unminimized)).generation ==
        swapchain_generation{2});

    fixture.state.resources = {};
}

TEST_CASE("[impure] pre-creation failures preserve unretired live swapchain targets")
{
    RecreationMockFixture fixture;
    char first_swapchain_storage{}, first_image_storage{}, first_view_storage{},
        first_pass_storage{}, first_framebuffer_storage{};
    VkSwapchainKHR first_swapchain = reinterpret_cast<VkSwapchainKHR>(&first_swapchain_storage);
    VkRenderPass first_render_pass = reinterpret_cast<VkRenderPass>(&first_pass_storage);
    VkResult support_status = VK_SUCCESS;
    VkResult image_format_status = VK_SUCCESS;
    SUBCASE("surface support failure")
    {
        support_status = VK_ERROR_SURFACE_LOST_KHR;
    }
    SUBCASE("unsupported image format")
    {
        image_format_status = VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
    auto plan = fixture.make_recreate_plan();
    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .expected_old_swapchain = VK_NULL_HANDLE,
            .new_swapchain = first_swapchain,
            .new_image = reinterpret_cast<VkImage>(&first_image_storage),
            .new_view = reinterpret_cast<VkImageView>(&first_view_storage),
            .new_render_pass = first_render_pass,
            .expected_framebuffer_pass = first_render_pass,
            .new_framebuffer = reinterpret_cast<VkFramebuffer>(&first_framebuffer_storage)},
        expectations);
    REQUIRE(
        fnfelt::result::is_success(run_recreation_state_io(fixture.runtime, plan, fixture.state)));

    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .support_status = support_status, .image_format_status = image_format_status},
        expectations);
    swapchain_recreation_result failure =
        run_recreation_state_io(fixture.runtime, plan, fixture.state);

    REQUIRE(fnfelt::result::is_error(failure));
    if (support_status != VK_SUCCESS)
    {
        CHECK_EQ(
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(failure)).cause.result,
            VK_ERROR_SURFACE_LOST_KHR);
    }
    else
    {
        CHECK(
            std::get<NoSuitableSwapchainConfiguration>(fnfelt::result::get_error(failure)).reason ==
            SwapchainConfigurationFailure::unsupported_image_parameters);
    }
    CHECK(
        fixture.state.resources.get().swapchain_targets.latest_generation ==
        swapchain_generation{1});
    REQUIRE(fixture.state.resources.get().swapchain_targets.live.has_value());
    CHECK_EQ(
        fixture.state.resources.get()
            .swapchain_targets.live->targets.presentation_image_views.presentation_swapchain_images
            .presentation_swapchain.swapchain.get(),
        first_swapchain);

    fixture.state.resources = {};
}

TEST_CASE("[impure] post-retirement failures clear live state and permit clean retry from null")
{
    RecreationMockFixture fixture;
    char first_swapchain_storage{}, second_swapchain_storage{}, third_swapchain_storage{},
        first_image_storage{}, second_image_storage{}, third_image_storage{}, first_view_storage{},
        second_view_storage{}, third_view_storage{}, first_pass_storage{}, second_pass_storage{},
        first_framebuffer_storage{}, retry_framebuffer_storage{};
    VkSwapchainKHR first_swapchain = reinterpret_cast<VkSwapchainKHR>(&first_swapchain_storage);
    VkSwapchainKHR second_swapchain = reinterpret_cast<VkSwapchainKHR>(&second_swapchain_storage);
    VkSwapchainKHR third_swapchain = reinterpret_cast<VkSwapchainKHR>(&third_swapchain_storage);
    VkRenderPass first_render_pass = reinterpret_cast<VkRenderPass>(&first_pass_storage);
    VkRenderPass second_render_pass = reinterpret_cast<VkRenderPass>(&second_pass_storage);

    bool should_fail_create_swapchain = false;
    bool should_fail_framebuffer_create = false;
    bool should_fail_framebuffer_extent = false;
    SUBCASE("vkCreateSwapchainKHR fails after retirement, retry from VK_NULL_HANDLE succeeds")
    {
        should_fail_create_swapchain = true;
    }
    SUBCASE("vkCreateFramebuffer fails after swapchain creation, new resources clean up")
    {
        should_fail_framebuffer_create = true;
    }
    SUBCASE("framebuffer physical extent exceeded cleans up newly created resources")
    {
        should_fail_framebuffer_extent = true;
    }

    std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
    auto plan = fixture.make_recreate_plan();
    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .expected_old_swapchain = VK_NULL_HANDLE,
            .new_swapchain = first_swapchain,
            .new_image = reinterpret_cast<VkImage>(&first_image_storage),
            .new_view = reinterpret_cast<VkImageView>(&first_view_storage),
            .new_render_pass = first_render_pass,
            .expected_framebuffer_pass = first_render_pass,
            .new_framebuffer = reinterpret_cast<VkFramebuffer>(&first_framebuffer_storage)},
        expectations);
    REQUIRE(
        fnfelt::result::is_success(run_recreation_state_io(fixture.runtime, plan, fixture.state)));

    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .width = 64,
            .height = 48,
            .expected_old_swapchain = first_swapchain,
            .new_swapchain = second_swapchain,
            .new_image = reinterpret_cast<VkImage>(&second_image_storage),
            .new_view = reinterpret_cast<VkImageView>(&second_view_storage),
            .new_render_pass = VK_NULL_HANDLE,
            .expected_framebuffer_pass = first_render_pass,
            .new_framebuffer = reinterpret_cast<VkFramebuffer>(&retry_framebuffer_storage),
            .create_swapchain_status =
                should_fail_create_swapchain ? VK_ERROR_OUT_OF_DATE_KHR : VK_SUCCESS,
            .create_framebuffer_status =
                should_fail_framebuffer_create ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS,
            .max_framebuffer_width = should_fail_framebuffer_extent ? 32U : 4096U,
            .should_create_render_pass = false},
        expectations);
    swapchain_recreation_result failed =
        run_recreation_state_io(fixture.runtime, plan, fixture.state);
    REQUIRE(fnfelt::result::is_error(failed));
    CHECK(
        fixture.state.resources.get().swapchain_targets.latest_generation ==
        swapchain_generation{2});
    CHECK_FALSE(fixture.state.resources.get().swapchain_targets.live.has_value());

    if (should_fail_create_swapchain)
    {
        CHECK_EQ(
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(failed)).cause.result,
            VK_ERROR_OUT_OF_DATE_KHR);

        expect_recreation_round(
            fixture.api,
            fixture,
            RecreationScript{
                .expected_old_swapchain = VK_NULL_HANDLE,
                .new_swapchain = third_swapchain,
                .new_image = reinterpret_cast<VkImage>(&third_image_storage),
                .new_view = reinterpret_cast<VkImageView>(&third_view_storage),
                .new_render_pass = second_render_pass,
                .expected_framebuffer_pass = second_render_pass,
                .new_framebuffer = reinterpret_cast<VkFramebuffer>(&retry_framebuffer_storage),
                .should_create_render_pass = true},
            expectations);
        swapchain_recreation_result retried =
            run_recreation_state_io(fixture.runtime, plan, fixture.state);
        REQUIRE(fnfelt::result::is_success(retried));
        CHECK(
            std::get<SwapchainRecreated>(fnfelt::result::get_success(retried)).generation ==
            swapchain_generation{3});
        REQUIRE(fixture.state.resources.get().swapchain_targets.live.has_value());
        CHECK_EQ(
            fixture.state.resources.get()
                .swapchain_targets.live->targets.presentation_image_views
                .presentation_swapchain_images.presentation_swapchain.swapchain.get(),
            third_swapchain);
    }
    else if (should_fail_framebuffer_create)
    {
        CHECK_EQ(
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(failed)).cause.operation,
            FallibleVulkanOperation::create_framebuffer);
        CHECK_EQ(
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(failed)).cause.result,
            VK_ERROR_OUT_OF_DEVICE_MEMORY);
    }
    else
    {
        CHECK_EQ(
            std::get<UnsupportedFramebufferExtent>(fnfelt::result::get_error(failed)).maximum.width,
            32U);
    }

    fixture.state.resources = {};
}

TEST_CASE("[impure] immutable snapshot keeps earlier swapchain generation alive until dropped")
{
    RecreationMockFixture fixture;
    char first_swapchain_storage{}, second_swapchain_storage{}, first_image_storage{},
        second_image_storage{}, first_view_storage{}, second_view_storage{}, first_pass_storage{},
        first_framebuffer_storage{}, second_framebuffer_storage{};
    VkSwapchainKHR first_swapchain = reinterpret_cast<VkSwapchainKHR>(&first_swapchain_storage);
    VkSwapchainKHR second_swapchain = reinterpret_cast<VkSwapchainKHR>(&second_swapchain_storage);
    VkRenderPass first_render_pass = reinterpret_cast<VkRenderPass>(&first_pass_storage);
    bool is_first_framebuffer_destroyed = false;
    bool is_first_view_destroyed = false;
    bool is_first_swapchain_destroyed = false;
    std::vector<std::string> gen1_cleanup;
    std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
    auto plan = fixture.make_recreate_plan();
    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .expected_old_swapchain = VK_NULL_HANDLE,
            .new_swapchain = first_swapchain,
            .new_image = reinterpret_cast<VkImage>(&first_image_storage),
            .new_view = reinterpret_cast<VkImageView>(&first_view_storage),
            .new_render_pass = first_render_pass,
            .expected_framebuffer_pass = first_render_pass,
            .new_framebuffer = reinterpret_cast<VkFramebuffer>(&first_framebuffer_storage),
            .on_destroy_view =
                [&]
            {
                is_first_view_destroyed = true;
                gen1_cleanup.emplace_back("view_1");
            },
            .on_destroy_swapchain =
                [&]
            {
                is_first_swapchain_destroyed = true;
                gen1_cleanup.emplace_back("swapchain_1");
            },
            .on_destroy_framebuffer =
                [&]
            {
                is_first_framebuffer_destroyed = true;
                gen1_cleanup.emplace_back("fb_1");
            }},
        expectations);
    REQUIRE(
        fnfelt::result::is_success(run_recreation_state_io(fixture.runtime, plan, fixture.state)));

    RecreationState<backend::MockApi> gen1_snapshot = fixture.state;
    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .expected_old_swapchain = first_swapchain,
            .new_swapchain = second_swapchain,
            .new_image = reinterpret_cast<VkImage>(&second_image_storage),
            .new_view = reinterpret_cast<VkImageView>(&second_view_storage),
            .new_render_pass = VK_NULL_HANDLE,
            .expected_framebuffer_pass = first_render_pass,
            .new_framebuffer = reinterpret_cast<VkFramebuffer>(&second_framebuffer_storage),
            .should_create_render_pass = false},
        expectations);
    REQUIRE(
        fnfelt::result::is_success(run_recreation_state_io(fixture.runtime, plan, fixture.state)));

    CHECK_FALSE(is_first_framebuffer_destroyed);
    CHECK_FALSE(is_first_view_destroyed);
    CHECK_FALSE(is_first_swapchain_destroyed);
    REQUIRE(gen1_snapshot.resources.get().swapchain_targets.live.has_value());
    CHECK(
        gen1_snapshot.resources.get().swapchain_targets.live->generation ==
        swapchain_generation{1});
    REQUIRE(fixture.state.resources.get().swapchain_targets.live.has_value());
    CHECK(
        fixture.state.resources.get().swapchain_targets.live->generation ==
        swapchain_generation{2});

    gen1_snapshot.resources = {};
    CHECK_EQ(gen1_cleanup, std::vector<std::string>{"fb_1", "view_1", "swapchain_1"});
    REQUIRE(fixture.state.resources.get().swapchain_targets.live.has_value());
    CHECK(
        fixture.state.resources.get().swapchain_targets.live->generation ==
        swapchain_generation{2});

    fixture.state.resources = {};
}

TEST_CASE("[impure] stale generation guard rejects replaced or retired targets before chaining")
{
    RecreationMockFixture fixture;
    char first_swapchain_storage{}, second_swapchain_storage{}, first_image_storage{},
        second_image_storage{}, first_view_storage{}, second_view_storage{}, first_pass_storage{},
        first_framebuffer_storage{}, second_framebuffer_storage{};
    VkSwapchainKHR first_swapchain = reinterpret_cast<VkSwapchainKHR>(&first_swapchain_storage);
    VkSwapchainKHR second_swapchain = reinterpret_cast<VkSwapchainKHR>(&second_swapchain_storage);
    VkRenderPass first_render_pass = reinterpret_cast<VkRenderPass>(&first_pass_storage);
    std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
    int continuation_calls = 0;
    auto check_gen1_and_count =
        state_io_require_live_targets<RecreationState<backend::MockApi>>(swapchain_generation{1})
            .and_then(
                [&](PresentationFramebuffers const &)
                {
                    ++continuation_calls;
                    return fnfelt::monad::state_io::just<RecreationState<backend::MockApi>>(
                        renderer_result<int>{fnfelt::result::success<renderer_result<int>>(7)});
                });

    auto plan = fixture.make_recreate_plan();
    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .expected_old_swapchain = VK_NULL_HANDLE,
            .new_swapchain = first_swapchain,
            .new_image = reinterpret_cast<VkImage>(&first_image_storage),
            .new_view = reinterpret_cast<VkImageView>(&first_view_storage),
            .new_render_pass = first_render_pass,
            .expected_framebuffer_pass = first_render_pass,
            .new_framebuffer = reinterpret_cast<VkFramebuffer>(&first_framebuffer_storage)},
        expectations);
    REQUIRE(
        fnfelt::result::is_success(run_recreation_state_io(fixture.runtime, plan, fixture.state)));

    renderer_result<int> live_check =
        run_recreation_state_io(fixture.runtime, check_gen1_and_count, fixture.state);
    REQUIRE(fnfelt::result::is_success(live_check));
    CHECK_EQ(continuation_calls, 1);

    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .expected_old_swapchain = first_swapchain,
            .new_swapchain = second_swapchain,
            .new_image = reinterpret_cast<VkImage>(&second_image_storage),
            .new_view = reinterpret_cast<VkImageView>(&second_view_storage),
            .new_render_pass = VK_NULL_HANDLE,
            .expected_framebuffer_pass = first_render_pass,
            .new_framebuffer = reinterpret_cast<VkFramebuffer>(&second_framebuffer_storage),
            .should_create_render_pass = false},
        expectations);
    REQUIRE(
        fnfelt::result::is_success(run_recreation_state_io(fixture.runtime, plan, fixture.state)));
    renderer_result<int> stale_check =
        run_recreation_state_io(fixture.runtime, check_gen1_and_count, fixture.state);
    REQUIRE(fnfelt::result::is_error(stale_check));
    CHECK_EQ(continuation_calls, 1);
    StaleSwapchainGeneration const & replaced_error =
        std::get<StaleSwapchainGeneration>(fnfelt::result::get_error(stale_check));
    CHECK(replaced_error.expected == swapchain_generation{1});
    REQUIRE(replaced_error.active.has_value());
    CHECK(*replaced_error.active == swapchain_generation{2});

    expect_recreation_round(
        fixture.api,
        fixture,
        RecreationScript{
            .expected_old_swapchain = second_swapchain,
            .create_swapchain_status = VK_ERROR_OUT_OF_DATE_KHR,
            .should_create_render_pass = false},
        expectations);
    REQUIRE(
        fnfelt::result::is_error(run_recreation_state_io(fixture.runtime, plan, fixture.state)));
    renderer_result<PresentationFramebuffers> missing_check = run_recreation_state_io(
        fixture.runtime,
        state_io_require_live_targets<RecreationState<backend::MockApi>>(swapchain_generation{2}),
        fixture.state);
    REQUIRE(fnfelt::result::is_error(missing_check));
    StaleSwapchainGeneration const & missing_error =
        std::get<StaleSwapchainGeneration>(fnfelt::result::get_error(missing_check));
    CHECK(missing_error.expected == swapchain_generation{2});
    CHECK_FALSE(missing_error.active.has_value());

    fixture.state.resources = {};
}

TEST_CASE(
    "[integration][windowed] swapchain recreation replaces live targets and retires prior "
    "generation cleanly")
{
    backend::Api api;
    using runtime_environment = sdl::detail::TestState<backend::Api>;
    using runtime_state = immer::box<runtime_environment>;
    sdl::detail::TestRuntime runtime{api};
    namespace result = fnfelt::result;
    REQUIRE(result::is_success(runtime.run(sdl::state_io_sdl_initialize_video<runtime_state>())));
    sdl::WindowOptions options{
        .width = sdl::window_width{64},
        .height = sdl::window_height{48},
        .flags = static_cast<SDL_WindowFlags>(
            SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE)};
    REQUIRE(
        result::is_success(runtime.run(sdl::state_io_sdl_create_window<runtime_state>(options))));
    renderer_result<PresentationInstance> instance = runtime.run(
        reader_io_create_presentation_instance<runtime_environment>(
            runtime.resources().window_owner));
    if (result::is_error(instance))
    {
        CHECK(std::holds_alternative<PresentationInstanceFailure>(result::get_error(instance)));
        return;
    }
    REQUIRE(result::is_success(instance));
    sdl::sdl_result<surface_ptr> surface = runtime.run(
        sdl::reader_io_sdl_vulkan_create_surface<runtime_environment>(
            runtime.resources().window_owner, result::get_success(instance).instance));
    REQUIRE(result::is_success(surface));
    presentation_device_result device = runtime.run(
        reader_io_create_presentation_device<runtime_environment>(
            result::get_success(instance), result::get_success(surface)));
    if (result::is_error(device))
    {
        CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(result::get_error(device)));
        return;
    }
    REQUIRE(result::is_success(device));
    PresentationDevice context = result::get_success(std::move(device));
    vulkan_result<command_pool_ptr> command_pool = runtime.run(
        reader_io_vk_create_command_pool<runtime_environment>(
            context.device, context.queue_families.graphics_family_index));
    REQUIRE(result::is_success(command_pool));

    RecreationState<backend::Api> state{std::ref(api), runtime.environment().main_context, {}};
    auto recreate = state_io_recreate_presentation_targets<RecreationState<backend::Api>>(
        context, runtime.resources().window_owner);
    auto present = reader_io_clear_and_present<IntegrationFrameEnvironment<backend::Api>>(
        VkClearColorValue{{0.1F, 0.2F, 0.3F, 1.0F}}, timeout_nanoseconds{1'000'000'000});

    swapchain_recreation_result gen1 = run_recreation_state_io(runtime, recreate, state);
    REQUIRE(result::is_success(gen1));
    REQUIRE(std::holds_alternative<SwapchainRecreated>(result::get_success(gen1)));
    CHECK(
        std::get<SwapchainRecreated>(result::get_success(gen1)).generation ==
        swapchain_generation{1});

    VkSwapchainKHR gen1_handle = VK_NULL_HANDLE;
    VkRenderPass gen1_pass = VK_NULL_HANDLE;
    VkFormat gen1_format = VK_FORMAT_UNDEFINED;
    std::weak_ptr<void> gen1_observer;
    {
        renderer_result<PresentationFramebuffers> gen1_targets = run_recreation_state_io(
            runtime,
            state_io_require_live_targets<RecreationState<backend::Api>>(swapchain_generation{1}),
            state);
        REQUIRE(result::is_success(gen1_targets));
        PresentationSwapchain const & gen1_swapchain =
            result::get_success(gen1_targets)
                .presentation_image_views.presentation_swapchain_images.presentation_swapchain;
        gen1_handle = gen1_swapchain.swapchain.get();
        gen1_pass = result::get_success(gen1_targets).render_pass.get();
        gen1_format = gen1_swapchain.configuration.surface_format.format;
        CHECK_GT(gen1_swapchain.configuration.extent.width, 0U);
        CHECK_GT(gen1_swapchain.configuration.extent.height, 0U);
        CHECK_FALSE(result::get_success(gen1_targets).framebuffers.empty());
        gen1_observer = gen1_swapchain.swapchain;

        completed_presentation_result gen1_frame = run_frame_with_targets(
            runtime,
            present,
            result::get_success(std::move(gen1_targets)),
            result::get_success(command_pool));
        REQUIRE(result::is_success(gen1_frame));
    }
    REQUIRE_FALSE(gen1_observer.expired());

    ::SDL_SetWindowSize(runtime.resources().window_owner.get(), 128, 96);
    [[maybe_unused]] immer::array<sdl::event> drained = runtime.run(
        sdl::reader_io_sdl_collect_pending_events<runtime_environment>(
            sdl::event_batch_limit{256}));

    swapchain_recreation_result gen2 = run_recreation_state_io(runtime, recreate, state);
    REQUIRE(result::is_success(gen2));
    REQUIRE(std::holds_alternative<SwapchainRecreated>(result::get_success(gen2)));
    CHECK(
        std::get<SwapchainRecreated>(result::get_success(gen2)).generation ==
        swapchain_generation{2});
    CHECK(gen1_observer.expired());

    renderer_result<PresentationFramebuffers> stale_gen1 = run_recreation_state_io(
        runtime,
        state_io_require_live_targets<RecreationState<backend::Api>>(swapchain_generation{1}),
        state);
    REQUIRE(result::is_error(stale_gen1));
    CHECK(
        std::get<StaleSwapchainGeneration>(result::get_error(stale_gen1)).expected ==
        swapchain_generation{1});
    REQUIRE(std::get<StaleSwapchainGeneration>(result::get_error(stale_gen1)).active.has_value());
    CHECK(
        *std::get<StaleSwapchainGeneration>(result::get_error(stale_gen1)).active ==
        swapchain_generation{2});

    {
        renderer_result<PresentationFramebuffers> gen2_targets = run_recreation_state_io(
            runtime,
            state_io_require_live_targets<RecreationState<backend::Api>>(swapchain_generation{2}),
            state);
        REQUIRE(result::is_success(gen2_targets));
        PresentationSwapchain const & gen2_swapchain =
            result::get_success(gen2_targets)
                .presentation_image_views.presentation_swapchain_images.presentation_swapchain;
        CHECK_NE(gen2_swapchain.swapchain.get(), gen1_handle);
        CHECK_GT(gen2_swapchain.configuration.extent.width, 0U);
        CHECK_GT(gen2_swapchain.configuration.extent.height, 0U);
        if (gen2_swapchain.configuration.surface_format.format == gen1_format)
        {
            CHECK_EQ(result::get_success(gen2_targets).render_pass.get(), gen1_pass);
        }

        completed_presentation_result gen2_frame = run_frame_with_targets(
            runtime,
            present,
            result::get_success(std::move(gen2_targets)),
            result::get_success(command_pool));
        REQUIRE(result::is_success(gen2_frame));
    }

    state.resources = {};
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
