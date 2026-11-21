// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_device.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <variant>

#include <SDL2/SDL_video.h>
#include <vulkan/vulkan_core.h>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>
#include <immer/box.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "../sdl/detail/test_runtime.hpp"
#include "../sdl/errors.hpp"
#include "../sdl/video.hpp"
#include "../sdl/vulkan_surface.hpp"
#include "../sdl/window.hpp"
#include "../sdl/window_types.hpp"
#include "../text/string.hpp"
#include "./extension_types.hpp"
#include "./managed_handles.hpp"
#include "./presentation_instance.hpp"
#include "./presentation_instance_types.hpp"
#include "./queue_types.hpp"
#include "./renderer_errors.hpp"

struct VkInstance_T
{
};
struct VkPhysicalDevice_T
{
};
struct VkDevice_T
{
};
struct VkQueue_T
{
};

namespace vulkandemo::vulkan
{
namespace
{
template <class TApi>
struct PresentationEnvironment
{
    std::reference_wrapper<TApi> api;
};

auto extension(std::string_view name) -> VkExtensionProperties
{
    VkExtensionProperties properties{};
    std::ranges::copy(name, properties.extensionName);
    return properties;
}

struct PresentationFixture
{
    backend::MockApi api;
    VkInstance_T instance_handle;
    VkPhysicalDevice_T physical_device;
    char surface_storage{};
    bool is_instance_destroyed = false;
    bool is_surface_destroyed = false;
    int destroyed_devices = 0;
    trompeloeil::sequence graphics_acquisitions;
    trompeloeil::sequence present_acquisitions;
    immer::array_transient<std::shared_ptr<trompeloeil::expectation>> expectations;
    instance_ptr instance;
    surface_ptr surface;

    PresentationFixture()
    {
        keep(NAMED_ALLOW_CALL(api, vk_destroy_device(trompeloeil::_, trompeloeil::_))
                 .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected device destruction")));
        keep(NAMED_REQUIRE_CALL(api, vk_destroy_instance(&instance_handle, nullptr))
                 .LR_SIDE_EFFECT(is_instance_destroyed = true));
        instance = make_instance_ptr(&instance_handle, api);
        surface = surface_ptr{
            reinterpret_cast<VkSurfaceKHR>(&surface_storage),
            [this, parent = instance](auto) mutable
            {
                is_surface_destroyed = true;
                parent.reset();
            }};
    }

    void keep(std::unique_ptr<trompeloeil::expectation> expectation)
    {
        expectations.push_back(std::shared_ptr<trompeloeil::expectation>{std::move(expectation)});
    }

    auto instance_context() const -> PresentationInstance
    {
        return {
            instance,
            immer::array{
                extension_name{VK_KHR_SURFACE_EXTENSION_NAME},
                extension_name{VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME},
                extension_name{"VK_KHR_surface_maintenance1"},
                extension_name{"VK_EXT_surface_maintenance1"}}};
    }

    template <std::size_t runs = 1>
    void selection(
        std::string_view maintenance1_name,
        bool has_shared_family = true,
        bool has_feature = true,
        VkResult enumeration_status = VK_SUCCESS,
        bool has_candidates = true,
        bool has_extra_extension = true,
        VkResult extension_status = VK_SUCCESS)
    {
        keep(NAMED_REQUIRE_CALL(
                 api, vk_enumerate_physical_devices(&instance_handle, trompeloeil::_, nullptr))
                 .LR_SIDE_EFFECT(CHECK_FALSE(is_instance_destroyed))
                 .LR_SIDE_EFFECT(CHECK_FALSE(is_surface_destroyed))
                 .SIDE_EFFECT(*_2 = has_candidates ? 1U : 0U)
                 .RETURN(enumeration_status)
                 .TIMES(runs));
        if (enumeration_status != VK_SUCCESS || !has_candidates)
        {
            return;
        }
        keep(NAMED_REQUIRE_CALL(
                 api,
                 vk_enumerate_physical_devices(&instance_handle, trompeloeil::_, trompeloeil::_))
                 .WITH(_3 != nullptr && *_2 == 1U)
                 .LR_SIDE_EFFECT(*_3 = &physical_device)
                 .RETURN(VK_SUCCESS)
                 .TIMES(runs));
        keep(NAMED_REQUIRE_CALL(
                 api, vk_get_physical_device_properties(&physical_device, trompeloeil::_))
                 .SIDE_EFFECT(_2->deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                 .SIDE_EFFECT(_2->apiVersion = VK_API_VERSION_1_1)
                 .TIMES(runs));
        VkQueueFamilyProperties graphics_properties{};
        graphics_properties.queueCount = 1;
        graphics_properties.queueFlags = VK_QUEUE_GRAPHICS_BIT;
        VkQueueFamilyProperties present_properties{};
        present_properties.queueCount = 1;
        immer::array<VkQueueFamilyProperties> families{
            VkQueueFamilyProperties{}, graphics_properties};
        if (!has_shared_family)
        {
            families = families.push_back(present_properties);
        }
        std::uint32_t const family_count = static_cast<std::uint32_t>(families.size());
        keep(NAMED_REQUIRE_CALL(
                 api,
                 vk_get_physical_device_queue_family_properties(
                     &physical_device, trompeloeil::_, nullptr))
                 .SIDE_EFFECT(*_2 = family_count)
                 .TIMES(runs));
        keep(NAMED_REQUIRE_CALL(
                 api,
                 vk_get_physical_device_queue_family_properties(
                     &physical_device, trompeloeil::_, trompeloeil::_))
                 .WITH(_3 != nullptr && *_2 == family_count)
                 .SIDE_EFFECT(std::ranges::copy(families, _3))
                 .TIMES(runs));
        immer::array available{
            extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME), extension(maintenance1_name)};
        if (has_extra_extension)
        {
            available = available.push_back(extension("VK_extra"));
        }
        keep(NAMED_REQUIRE_CALL(
                 api,
                 vk_enumerate_device_extension_properties(
                     &physical_device, nullptr, trompeloeil::_, nullptr))
                 .SIDE_EFFECT(*_3 = static_cast<std::uint32_t>(available.size()))
                 .RETURN(extension_status)
                 .TIMES(runs));
        if (extension_status == VK_SUCCESS)
        {
            keep(NAMED_REQUIRE_CALL(
                     api,
                     vk_enumerate_device_extension_properties(
                         &physical_device, nullptr, trompeloeil::_, trompeloeil::_))
                     .WITH(_4 != nullptr && *_3 == available.size())
                     .SIDE_EFFECT(std::ranges::copy(available, _4))
                     .RETURN(VK_SUCCESS)
                     .TIMES(runs));
        }
        for (std::uint32_t family = 0; family < family_count; ++family)
        {
            bool const has_presentation = has_shared_family ? family == 1U : family == 2U;
            keep(NAMED_REQUIRE_CALL(
                     api,
                     vk_get_physical_device_surface_support_khr(
                         &physical_device, family, surface.get(), trompeloeil::_))
                     .SIDE_EFFECT(*_4 = has_presentation ? VK_TRUE : VK_FALSE)
                     .RETURN(VK_SUCCESS)
                     .TIMES(runs));
        }
        if (extension_status != VK_SUCCESS)
        {
            return;
        }
        keep(NAMED_REQUIRE_CALL(
                 api, vk_get_physical_device_features2(&physical_device, trompeloeil::_))
                 .SIDE_EFFECT(
                     static_cast<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT *>(_2->pNext)
                         ->swapchainMaintenance1 = has_feature ? VK_TRUE : VK_FALSE)
                 .TIMES(runs));
    }

    void create(VkDevice device, std::string_view, VkResult status = VK_SUCCESS)
    {
        keep(NAMED_REQUIRE_CALL(
                 api, vk_create_device(&physical_device, trompeloeil::_, nullptr, trompeloeil::_))
                 .LR_SIDE_EFFECT(CHECK_FALSE(is_instance_destroyed))
                 .LR_SIDE_EFFECT(CHECK_FALSE(is_surface_destroyed))
                 .WITH(_2 != nullptr && _2->pNext == nullptr && _4 != nullptr)
                 .WITH(_2->enabledExtensionCount == 2U)
                 .WITH(
                     std::string_view{_2->ppEnabledExtensionNames[0]} ==
                     VK_KHR_SWAPCHAIN_EXTENSION_NAME)
                 .WITH(std::string_view{_2->ppEnabledExtensionNames[1]} == "VK_extra")
                 .SIDE_EFFECT(*_4 = device)
                 .RETURN(status)
                 .IN_SEQUENCE(graphics_acquisitions, present_acquisitions));
        if (status == VK_SUCCESS)
        {
            keep(NAMED_REQUIRE_CALL(api, vk_destroy_device(device, nullptr))
                     .LR_SIDE_EFFECT(++destroyed_devices));
        }
    }

    void queues(
        VkDevice device, VkQueue graphics_queue, VkQueue present_queue, bool has_shared_family)
    {
        keep(NAMED_REQUIRE_CALL(api, vk_get_device_queue(device, 1, 0, trompeloeil::_))
                 .SIDE_EFFECT(*_4 = graphics_queue)
                 .IN_SEQUENCE(graphics_acquisitions));
        keep(NAMED_REQUIRE_CALL(
                 api, vk_get_device_queue(device, has_shared_family ? 1U : 2U, 0, trompeloeil::_))
                 .SIDE_EFFECT(*_4 = present_queue)
                 .IN_SEQUENCE(present_acquisitions));
    }
};
}  // namespace

TEST_CASE("[impure] presentation device plans are lazy and retain both parents")
{
    PresentationFixture fixture;
    PresentationEnvironment environment{std::ref(fixture.api)};

    {
        auto plan = reader_io_create_presentation_device<decltype(environment)>(
            fixture.instance_context(), std::move(fixture.surface));
        fixture.instance.reset();
        [[maybe_unused]] auto deferred = plan.build(environment);

        // No discovery or acquisition expectation exists until the program executes.
        CHECK_FALSE(fixture.is_instance_destroyed);
        CHECK_FALSE(fixture.is_surface_destroyed);
    }

    CHECK(fixture.is_instance_destroyed);
    CHECK(fixture.is_surface_destroyed);
}

TEST_CASE("[impure] presentation setup creates independent presentation devices")
{
    bool has_shared_family = true;
    std::string_view maintenance1_name = "VK_KHR_swapchain_maintenance1";
    SUBCASE("KHR with shared queue roles") {}
    SUBCASE("EXT with separate queue roles")
    {
        has_shared_family = false;
        maintenance1_name = "VK_EXT_swapchain_maintenance1";
    }
    PresentationFixture fixture;
    PresentationEnvironment environment{std::ref(fixture.api)};
    VkDevice_T first_device;
    VkDevice_T second_device;
    VkQueue_T graphics_queue;
    VkQueue_T separate_present_queue;
    VkQueue present_queue = has_shared_family ? &graphics_queue : &separate_present_queue;
    fixture.selection<2>(maintenance1_name, has_shared_family);
    fixture.create(&first_device, maintenance1_name);
    fixture.queues(&first_device, &graphics_queue, present_queue, has_shared_family);
    fixture.create(&second_device, maintenance1_name);
    fixture.queues(&second_device, &graphics_queue, present_queue, has_shared_family);
    auto plan = reader_io_create_presentation_device<decltype(environment)>(
        fixture.instance_context(), fixture.surface, {text::String{"VK_extra"}});

    presentation_device_result first = plan.build(environment).sync_wait(lf::lazy_pool{2});
    presentation_device_result second =
        std::move(plan).build(environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(first));
    REQUIRE(fnfelt::result::is_success(second));
    PresentationDevice const & context = fnfelt::result::get_success(first);
    CHECK_EQ(context.device.get(), &first_device);
    CHECK_EQ(fnfelt::result::get_success(second).device.get(), &second_device);
    CHECK_EQ(context.surface, fixture.surface);
    CHECK_EQ(context.physical_device, &fixture.physical_device);
    CHECK_EQ(context.queue_families.graphics_family_index, queue_family_index{1});
    CHECK_EQ(
        context.queue_families.present_family_index,
        queue_family_index{has_shared_family ? 1U : 2U});
    CHECK_EQ(context.graphics_queue, &graphics_queue);
    CHECK_EQ(context.present_queue, present_queue);
}

TEST_CASE("[impure] presentation device failures bypass later acquisition")
{
    bool has_feature = true;
    bool has_candidates = true;
    bool has_extra_extension = true;
    VkResult enumeration_status = VK_SUCCESS;
    VkResult extension_status = VK_SUCCESS;
    VkResult creation_status = VK_SUCCESS;
    SUBCASE("physical-device enumeration fails")
    {
        enumeration_status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    SUBCASE("no candidate has the required feature")
    {
        has_feature = false;
    }
    SUBCASE("no physical devices exist")
    {
        has_candidates = false;
    }
    SUBCASE("a required additional extension is absent")
    {
        has_extra_extension = false;
    }
    SUBCASE("extension query fails")
    {
        extension_status = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    SUBCASE("logical-device creation fails")
    {
        creation_status = VK_ERROR_FEATURE_NOT_PRESENT;
    }
    PresentationFixture fixture;
    PresentationEnvironment environment{std::ref(fixture.api)};
    VkDevice_T device;
    fixture.selection(
        "VK_EXT_swapchain_maintenance1",
        true,
        has_feature,
        enumeration_status,
        has_candidates,
        has_extra_extension,
        extension_status);
    if (creation_status != VK_SUCCESS)
    {
        fixture.create(&device, "VK_EXT_swapchain_maintenance1", creation_status);
    }

    presentation_device_result outcome =
        reader_io_create_presentation_device<decltype(environment)>(
            fixture.instance_context(), fixture.surface, {text::String{"VK_extra"}})
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    // Only the final subcase permits creation; none permits a subsequent queue lookup.
    REQUIRE(fnfelt::result::is_error(outcome));
    renderer_error const & failure = fnfelt::result::get_error(outcome);
    if (!has_feature || !has_candidates || !has_extra_extension)
    {
        CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(failure));
    }
    else
    {
        REQUIRE(std::holds_alternative<VulkanOperationFailure>(failure));
        CHECK_EQ(
            std::get<VulkanOperationFailure>(failure).cause.result,
            enumeration_status != VK_SUCCESS     ? enumeration_status
                : extension_status != VK_SUCCESS ? extension_status
                                                 : creation_status);
    }
    CHECK_EQ(fixture.destroyed_devices, 0);
}

TEST_CASE("[impure] materialized presentation setup retains device and parents")
{
    PresentationFixture fixture;
    PresentationEnvironment environment{std::ref(fixture.api)};
    VkDevice_T device;
    VkQueue_T queue;
    fixture.selection("VK_EXT_swapchain_maintenance1");
    fixture.create(&device, "VK_EXT_swapchain_maintenance1");
    fixture.queues(&device, &queue, &queue, true);
    std::weak_ptr<VkInstance_T> instance_observer = fixture.instance;
    std::weak_ptr<surface_ptr::element_type> surface_observer = fixture.surface;
    PresentationDevice owner;
    std::weak_ptr<VkDevice_T> device_observer;

    {
        auto operation =
            reader_io_create_presentation_device<decltype(environment)>(
                fixture.instance_context(), fixture.surface, {text::String{"VK_extra"}})
                .build(environment)
                .io();
        fixture.instance.reset();
        fixture.surface.reset();
        presentation_device_result outcome = operation.build().sync_wait(lf::lazy_pool{2});
        REQUIRE(fnfelt::result::is_success(outcome));
        owner = fnfelt::result::get_success(outcome);
        device_observer = owner.device;
    }
    // All plans and IOs are gone: only the returned context now retains the native parents.
    CHECK_FALSE(instance_observer.expired());
    CHECK_FALSE(surface_observer.expired());
    CHECK_EQ(fixture.destroyed_devices, 0);
    owner = {};
    CHECK(device_observer.expired());
    CHECK(surface_observer.expired());
    CHECK(instance_observer.expired());
    CHECK_EQ(fixture.destroyed_devices, 1);
}

TEST_CASE("[integration][windowed] a presentation device retains its selected owners")
{
    backend::Api api;
    using environment_type = sdl::detail::TestState<backend::Api>;
    using state_type = immer::box<environment_type>;
    sdl::detail::TestRuntime runtime{api};
    REQUIRE(
        fnfelt::result::is_success(runtime.run(sdl::state_io_sdl_initialize_video<state_type>())));
    sdl::WindowOptions options{
        .width = sdl::window_width{64},
        .height = sdl::window_height{48},
        .flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)};
    sdl::sdl_result<SDL_Window *> window =
        runtime.run(sdl::state_io_sdl_create_window<state_type>(options));
    REQUIRE(fnfelt::result::is_success(window));
    renderer_result<PresentationInstance> instance = runtime.run(
        reader_io_create_presentation_instance<environment_type>(runtime.resources().window_owner));
    if (fnfelt::result::is_error(instance))
    {
        CHECK(
            std::holds_alternative<PresentationInstanceFailure>(
                fnfelt::result::get_error(instance)));
        return;
    }
    REQUIRE(fnfelt::result::is_success(instance));
    PresentationInstance context = fnfelt::result::get_success(instance);
    sdl::sdl_result<surface_ptr> surface = runtime.run(
        sdl::reader_io_sdl_vulkan_create_surface<environment_type>(
            runtime.resources().window_owner, context.instance));
    REQUIRE(fnfelt::result::is_success(surface));

    presentation_device_result outcome = runtime.run(
        reader_io_create_presentation_device<environment_type>(
            context, fnfelt::result::get_success(surface)));

    if (fnfelt::result::is_error(outcome))
    {
        CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(fnfelt::result::get_error(outcome)));
        return;
    }
    REQUIRE(fnfelt::result::is_success(outcome));
    PresentationDevice const & created = fnfelt::result::get_success(outcome);
    CHECK_NE(created.device.get(), VK_NULL_HANDLE);
    CHECK_NE(created.graphics_queue, VK_NULL_HANDLE);
    CHECK_NE(created.present_queue, VK_NULL_HANDLE);
    CHECK_EQ(created.surface, fnfelt::result::get_success(surface));
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
