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
#include "../sdl/instance_extensions.hpp"
#include "../sdl/video.hpp"
#include "../sdl/vulkan_surface.hpp"
#include "../sdl/window.hpp"
#include "../sdl/window_types.hpp"
#include "../text/string.hpp"
#include "./errors.hpp"
#include "./instance.hpp"
#include "./managed_handles.hpp"
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
    bool is_device_destroyed = false;
    trompeloeil::sequence graphics_acquisitions;
    trompeloeil::sequence present_acquisitions;
    immer::array_transient<std::shared_ptr<trompeloeil::expectation>> expectations;
    instance_ptr instance;
    surface_ptr surface;

    PresentationFixture()
    {
        keep(NAMED_ALLOW_CALL(api, vk_destroy_device(trompeloeil::_, trompeloeil::_))
                 .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected device destruction")));
        keep(NAMED_ALLOW_CALL(api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
                 .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected instance destruction")));
        keep(NAMED_REQUIRE_CALL(api, vk_destroy_instance(&instance_handle, nullptr))
                 .LR_SIDE_EFFECT(is_instance_destroyed = true));
        instance = make_instance_ptr(&instance_handle, api);
        // Surface dispatch has separate tests; this owner isolates context retention here.
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

    template <std::size_t runs = 1>
    void enumerate(bool has_device = true, VkResult status = VK_SUCCESS)
    {
        keep(NAMED_REQUIRE_CALL(
                 api, vk_enumerate_physical_devices(&instance_handle, trompeloeil::_, nullptr))
                 .SIDE_EFFECT(*_2 = has_device ? 1U : 0U)
                 .RETURN(status)
                 .TIMES(runs));
        if (has_device && status == VK_SUCCESS)
        {
            keep(
                NAMED_REQUIRE_CALL(
                    api,
                    vk_enumerate_physical_devices(&instance_handle, trompeloeil::_, trompeloeil::_))
                    .WITH(_3 != nullptr && *_2 == 1U)
                    .LR_SIDE_EFFECT(*_3 = &physical_device)
                    .RETURN(VK_SUCCESS)
                    .TIMES(runs));
        }
    }

    template <std::size_t runs = 1>
    void assess(
        bool has_shared_family = true,
        bool has_extra_extension = true,
        VkResult extension_status = VK_SUCCESS)
    {
        // Family zero is unused; the selected roles must retain their actual snapshot indices.
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
        immer::array<VkExtensionProperties> available{extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)};
        if (has_extra_extension)
        {
            available = available.push_back(extension("VK_extra"));
        }
        keep(NAMED_REQUIRE_CALL(
                 api, vk_get_physical_device_properties(&physical_device, trompeloeil::_))
                 .SIDE_EFFECT(_2->deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                 .TIMES(runs));
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
    }

    void create(VkDevice device, bool has_shared_family, VkResult status = VK_SUCCESS)
    {
        keep(NAMED_REQUIRE_CALL(
                 api, vk_create_device(&physical_device, trompeloeil::_, nullptr, trompeloeil::_))
                 .WITH(_2 != nullptr && _4 != nullptr)
                 .WITH(_2->queueCreateInfoCount == (has_shared_family ? 1U : 2U))
                 .WITH(_2->pQueueCreateInfos[0].queueFamilyIndex == 1)
                 .WITH(has_shared_family || _2->pQueueCreateInfos[1].queueFamilyIndex == 2)
                 .WITH(_2->enabledExtensionCount == 2)
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
                     .LR_SIDE_EFFECT(is_device_destroyed = true));
        }
    }

    void queues(
        VkDevice device, VkQueue graphics_queue, VkQueue present_queue, bool has_shared_family)
    {
        // Both queue lookups follow creation and may complete in either order.
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
            std::move(fixture.instance), std::move(fixture.surface));
        [[maybe_unused]] auto deferred = plan.build(environment);

        // No discovery or acquisition expectation exists until the program executes.
        CHECK_FALSE(fixture.is_instance_destroyed);
        CHECK_FALSE(fixture.is_surface_destroyed);
    }

    CHECK(fixture.is_instance_destroyed);
    CHECK(fixture.is_surface_destroyed);
}

TEST_CASE("[impure] presentation device setup preserves shared and separate queue roles")
{
    bool has_shared_family = true;
    SUBCASE("one family supplies both roles") {}
    SUBCASE("separate families supply each role")
    {
        has_shared_family = false;
    }
    PresentationFixture fixture;
    PresentationEnvironment environment{std::ref(fixture.api)};
    VkDevice_T device;
    VkQueue_T graphics_queue;
    VkQueue_T other_present_queue;
    VkQueue const present_queue = has_shared_family ? &graphics_queue : &other_present_queue;
    fixture.enumerate();
    fixture.assess(has_shared_family);
    fixture.create(&device, has_shared_family);
    fixture.queues(&device, &graphics_queue, present_queue, has_shared_family);

    presentation_device_result outcome =
        reader_io_create_presentation_device<decltype(environment)>(
            fixture.instance,
            fixture.surface,
            {text::String{"VK_extra"}, text::String{VK_KHR_SWAPCHAIN_EXTENSION_NAME}})
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    PresentationDevice const & context = fnfelt::result::get_success(outcome);
    CHECK_EQ(context.device.get(), &device);
    CHECK_EQ(context.surface, fixture.surface);
    CHECK_EQ(context.physical_device, &fixture.physical_device);
    CHECK_EQ(context.queue_families.graphics_family_index, queue_family_index{1});
    CHECK_EQ(
        context.queue_families.present_family_index,
        queue_family_index{has_shared_family ? 1U : 2U});
    CHECK_EQ(context.graphics_queue, &graphics_queue);
    CHECK_EQ(context.present_queue, present_queue);
}

TEST_CASE("[impure] presentation device errors bypass subsequent acquisition effects")
{
    PresentationFixture fixture;
    PresentationEnvironment environment{std::ref(fixture.api)};
    VkDevice_T device;
    VkResult enumeration_status = VK_SUCCESS;
    VkResult extension_status = VK_SUCCESS;
    bool has_extra_extension = true;
    bool has_candidates = true;
    bool should_create_device = false;
    SUBCASE("enumeration fails")
    {
        enumeration_status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    SUBCASE("no physical devices exist")
    {
        has_candidates = false;
    }
    SUBCASE("extension query fails")
    {
        extension_status = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    SUBCASE("a required additional extension is absent")
    {
        has_extra_extension = false;
    }
    SUBCASE("logical device creation fails")
    {
        should_create_device = true;
    }
    fixture.enumerate(has_candidates, enumeration_status);
    if (has_candidates && enumeration_status == VK_SUCCESS)
    {
        fixture.assess(true, has_extra_extension, extension_status);
    }
    if (should_create_device)
    {
        fixture.create(&device, true, VK_ERROR_INITIALIZATION_FAILED);
    }

    presentation_device_result outcome =
        reader_io_create_presentation_device<decltype(environment)>(
            fixture.instance, fixture.surface, {text::String{"VK_extra"}})
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    // There are no queue expectations, and only the final subcase permits device creation.
    REQUIRE(fnfelt::result::is_error(outcome));
    renderer_error const & failure = fnfelt::result::get_error(outcome);
    if (!has_candidates || !has_extra_extension)
    {
        CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(failure));
    }
    else
    {
        REQUIRE(std::holds_alternative<VulkanOperationFailure>(failure));
        VulkanOperationFailure const & operation_failure =
            std::get<VulkanOperationFailure>(failure);
        CHECK_EQ(
            operation_failure.cause.result,
            should_create_device                   ? VK_ERROR_INITIALIZATION_FAILED
                : enumeration_status != VK_SUCCESS ? enumeration_status
                                                   : extension_status);
    }
    CHECK_FALSE(fixture.is_device_destroyed);
}

TEST_CASE("[impure] presentation context retains owners after plans and weak observers expire")
{
    PresentationFixture fixture;
    PresentationEnvironment environment{std::ref(fixture.api)};
    VkDevice_T device;
    VkQueue_T queue;
    fixture.enumerate();
    fixture.assess();
    fixture.create(&device, true);
    fixture.queues(&device, &queue, &queue, true);
    presentation_device_result outcome;

    {
        auto plan = reader_io_create_presentation_device<decltype(environment)>(
            std::move(fixture.instance), std::move(fixture.surface), {text::String{"VK_extra"}});

        outcome = plan.build(environment).sync_wait(lf::lazy_pool{2});
    }

    REQUIRE(fnfelt::result::is_success(outcome));
    PresentationDevice retained = fnfelt::result::get_success(outcome);
    std::weak_ptr<VkDevice_T> device_observer = retained.device;
    std::weak_ptr<surface_ptr::element_type> surface_observer = retained.surface;
    outcome = fnfelt::result::stopped<presentation_device_result>();

    // Only the returned context now keeps the device, surface and transitive instance alive.
    CHECK_FALSE(fixture.is_device_destroyed);
    CHECK_FALSE(fixture.is_surface_destroyed);
    CHECK_FALSE(fixture.is_instance_destroyed);

    retained = {};

    CHECK(fixture.is_device_destroyed);
    CHECK(fixture.is_surface_destroyed);
    CHECK(fixture.is_instance_destroyed);
    CHECK(device_observer.expired());
    CHECK(surface_observer.expired());
}

TEST_CASE("[impure] repeated presentation setup creates independent device owners")
{
    PresentationFixture fixture;
    PresentationEnvironment environment{std::ref(fixture.api)};
    VkDevice_T first_device;
    VkDevice_T second_device;
    VkQueue_T first_queue;
    VkQueue_T second_queue;
    fixture.enumerate<2>();
    fixture.assess<2>();
    fixture.create(&first_device, true);
    fixture.queues(&first_device, &first_queue, &first_queue, true);
    fixture.create(&second_device, true);
    fixture.queues(&second_device, &second_queue, &second_queue, true);
    auto plan = reader_io_create_presentation_device<decltype(environment)>(
        fixture.instance, fixture.surface, {text::String{"VK_extra"}});

    presentation_device_result first = plan.build(environment).sync_wait(lf::lazy_pool{2});
    presentation_device_result second = plan.build(environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(first));
    REQUIRE(fnfelt::result::is_success(second));
    CHECK_EQ(fnfelt::result::get_success(first).device.get(), &first_device);
    CHECK_EQ(fnfelt::result::get_success(second).device.get(), &second_device);
    CHECK_EQ(fnfelt::result::get_success(first).graphics_queue, &first_queue);
    CHECK_EQ(fnfelt::result::get_success(second).present_queue, &second_queue);
}

TEST_CASE("[integration][windowed] an SDL surface supports an owned presentation device")
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
    sdl::sdl_result<immer::array<text::String>> extensions = runtime.run(
        sdl::reader_io_sdl_vulkan_get_instance_extensions<environment_type>(
            fnfelt::result::get_success(window)));
    REQUIRE(fnfelt::result::is_success(extensions));
    vulkan_result<instance_ptr> instance = runtime.run(
        reader_io_vk_create_instance<environment_type>(fnfelt::result::get_success(extensions)));
    REQUIRE(fnfelt::result::is_success(instance));
    sdl::sdl_result<surface_ptr> surface = runtime.run(
        sdl::reader_io_sdl_vulkan_create_surface<environment_type>(
            runtime.resources().window_owner, fnfelt::result::get_success(instance)));
    REQUIRE(fnfelt::result::is_success(surface));

    presentation_device_result outcome = runtime.run(
        reader_io_create_presentation_device<environment_type>(
            fnfelt::result::get_success(instance), fnfelt::result::get_success(surface)));

    REQUIRE(fnfelt::result::is_success(outcome));
    PresentationDevice const & context = fnfelt::result::get_success(outcome);
    CHECK_NE(context.device.get(), VK_NULL_HANDLE);
    CHECK_NE(context.graphics_queue, VK_NULL_HANDLE);
    CHECK_NE(context.present_queue, VK_NULL_HANDLE);
    CHECK_EQ(context.surface, fnfelt::result::get_success(surface));
    if (context.queue_families.graphics_family_index == context.queue_families.present_family_index)
    {
        CHECK_EQ(context.graphics_queue, context.present_queue);
    }
    // No queue work or device children were created; owners can be released at scope exit.
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
