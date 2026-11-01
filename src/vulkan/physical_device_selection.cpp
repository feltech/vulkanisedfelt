// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./physical_device_selection.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "./physical_device_selection_types.hpp"
#include "./physical_devices_types.hpp"
#include "./queue_family_selection.hpp"
#include "./queue_types.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <memory>
#include <type_traits>
#include <variant>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./api_version_types.hpp"
#include "./instance.hpp"
#include "./managed_handles.hpp"
#include "./queue_family_properties.hpp"
#include "./renderer_errors.hpp"

struct VkInstance_T
{
};
struct VkPhysicalDevice_T
{
};
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan
{
auto score_physical_devices(immer::array<VkPhysicalDeviceProperties> const & properties)
    -> immer::array<PhysicalDeviceScore>
{
    auto candidates =
        properties | std::views::enumerate |
        std::views::transform(
            [](auto const & entry)
            {
                auto const & [index, device] = entry;
                return PhysicalDeviceScore{
                    physical_device_index{static_cast<std::uint32_t>(index)},
                    physical_device_score{
                        device.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 0U : 1U}};
            });
    auto scores = immer::array<PhysicalDeviceScore>{}.transient();
    std::ranges::copy(candidates, std::back_inserter(scores));
    return std::move(scores).persistent();
}

auto select_physical_device(immer::array<PhysicalDeviceScore> const & scores)
    -> std::optional<physical_device_index>
{
    auto selected = std::ranges::min_element(scores, {}, &PhysicalDeviceScore::score);
    if (selected == scores.end())
    {
        return std::nullopt;
    }
    return selected->device_index;
}
}  // namespace vulkandemo::vulkan

#ifndef DOCTEST_CONFIG_DISABLE
namespace vulkandemo::vulkan
{
namespace
{
static_assert(!std::is_convertible_v<std::uint32_t, physical_device_index>);
static_assert(!std::is_convertible_v<physical_device_index, std::uint32_t>);
static_assert(!std::is_convertible_v<physical_device_index, physical_device_score>);
static_assert(!std::is_convertible_v<physical_device_score, physical_device_index>);

constexpr auto device_properties(VkPhysicalDeviceType device_type) -> VkPhysicalDeviceProperties
{
    VkPhysicalDeviceProperties properties{};
    properties.apiVersion = VK_API_VERSION_1_1;
    properties.deviceType = device_type;
    return properties;
}
}  // namespace

TEST_CASE("[pure] physical device scoring accepts an empty snapshot")
{
    immer::array<VkPhysicalDeviceProperties> properties;

    immer::array scores = score_physical_devices(properties);

    CHECK(scores.empty());
}

TEST_CASE("[pure] physical device scoring prefers discrete GPUs and preserves indices")
{
    // Device order is retained so scores continue to identify the original snapshots.
    immer::array<VkPhysicalDeviceProperties> properties{
        device_properties(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_CPU),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_OTHER),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)};

    immer::array scores = score_physical_devices(properties);

    REQUIRE_EQ(scores.size(), properties.size());
    for (std::uint32_t index = 0; index < scores.size(); ++index)
    {
        CHECK_EQ(scores[index].device_index, physical_device_index{index});
    }
    CHECK_EQ(scores[0].score, physical_device_score{1});
    CHECK_EQ(scores[1].score, physical_device_score{0});
    CHECK_EQ(scores[2].score, physical_device_score{1});
    CHECK_EQ(scores[3].score, physical_device_score{1});
    CHECK_EQ(scores[4].score, physical_device_score{1});
    CHECK_EQ(scores[5].score, physical_device_score{0});
}

TEST_CASE("[pure] physical device scoring treats non-discrete types equally")
{
    // Keep a shared snapshot live while producing an independent score array.
    immer::array<VkPhysicalDeviceProperties> properties{
        device_properties(VK_PHYSICAL_DEVICE_TYPE_CPU),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)};
    immer::array retained{properties};

    immer::array scores = score_physical_devices(properties);

    REQUIRE_EQ(scores.size(), 2U);
    CHECK_EQ(scores[0].score, scores[1].score);
    CHECK_EQ(scores[0].score, physical_device_score{1});
    CHECK_EQ(retained[0].deviceType, VK_PHYSICAL_DEVICE_TYPE_CPU);
    CHECK_EQ(retained[1].deviceType, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
}

TEST_CASE("[pure] physical device selection accepts empty scores")
{
    immer::array<PhysicalDeviceScore> scores;

    std::optional<physical_device_index> selected_device_index = select_physical_device(scores);

    CHECK_FALSE(selected_device_index.has_value());
}

TEST_CASE("[pure] physical device selection uses supplied scores and original indices")
{
    // A custom policy can rank candidates independently of their array positions.
    immer::array<PhysicalDeviceScore> scores{
        {physical_device_index{9}, physical_device_score{4}},
        {physical_device_index{7}, physical_device_score{1}},
        {physical_device_index{2}, physical_device_score{1}}};

    std::optional<physical_device_index> selected_device_index = select_physical_device(scores);

    REQUIRE(selected_device_index.has_value());
    CHECK_EQ(*selected_device_index, physical_device_index{7});
}

TEST_CASE("[pure] physical device selection accepts maximum scores")
{
    // No numeric score is reserved as an ineligible-candidate sentinel.
    immer::array<PhysicalDeviceScore> scores{
        {physical_device_index{5}, physical_device_score{UINT32_MAX}},
        {physical_device_index{3}, physical_device_score{UINT32_MAX}}};

    std::optional<physical_device_index> selected_device_index = select_physical_device(scores);

    REQUIRE(selected_device_index.has_value());
    CHECK_EQ(*selected_device_index, physical_device_index{5});
}

TEST_CASE("[pure] physical device scoring and selection preserve policy and ties")
{
    immer::array<VkPhysicalDeviceProperties> properties{
        device_properties(VK_PHYSICAL_DEVICE_TYPE_CPU),
        device_properties(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)};
    physical_device_index expected_index{0};
    SUBCASE("discrete GPUs are preferred")
    {
        properties = properties.push_back(device_properties(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU));
        expected_index = physical_device_index{2};
    }
    SUBCASE("equally ranked non-discrete GPUs retain snapshot order") {}

    std::optional<physical_device_index> selected_device_index =
        select_physical_device(score_physical_devices(properties));

    REQUIRE(selected_device_index.has_value());
    CHECK_EQ(*selected_device_index, expected_index);
}

namespace
{
template <class TApi>
struct SelectionEnvironment
{
    std::reference_wrapper<TApi> api;
};

struct SelectionFixture
{
    backend::MockApi api;
    VkInstance_T handle;
    immer::array_transient<std::shared_ptr<trompeloeil::expectation>> expectations;
    instance_ptr instance;

    SelectionFixture()
    {
        keep(NAMED_ALLOW_CALL(api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
                 .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected instance destruction")));
        keep(NAMED_REQUIRE_CALL(api, vk_destroy_instance(&handle, nullptr)));
        instance = make_instance_ptr(&handle, api);
    }

    void keep(std::unique_ptr<trompeloeil::expectation> expectation)
    {
        expectations.push_back(std::shared_ptr<trompeloeil::expectation>{std::move(expectation)});
    }

    void devices(immer::array<VkPhysicalDevice> handles, VkResult status = VK_SUCCESS)
    {
        keep(
            NAMED_REQUIRE_CALL(api, vk_enumerate_physical_devices(&handle, trompeloeil::_, nullptr))
                .SIDE_EFFECT(*_2 = static_cast<std::uint32_t>(handles.size()))
                .RETURN(status));
        if (status == VK_SUCCESS && !handles.empty())
        {
            keep(NAMED_REQUIRE_CALL(
                     api, vk_enumerate_physical_devices(&handle, trompeloeil::_, trompeloeil::_))
                     .WITH(_3 != nullptr)
                     .SIDE_EFFECT(std::ranges::copy(handles, _3))
                     .RETURN(VK_SUCCESS));
        }
    }

    void probe(
        VkPhysicalDevice device,
        VkPhysicalDeviceType type,
        immer::array<VkQueueFamilyProperties> queues,
        immer::array<VkExtensionProperties> extensions = {},
        VkResult status = VK_SUCCESS,
        api_version version = minimum_api_version)
    {
        keep(NAMED_REQUIRE_CALL(api, vk_get_physical_device_properties(device, trompeloeil::_))
                 .SIDE_EFFECT(_2->deviceType = type)
                 .SIDE_EFFECT(_2->apiVersion = value_of(version)));
        keep(NAMED_REQUIRE_CALL(
                 api,
                 vk_get_physical_device_queue_family_properties(device, trompeloeil::_, nullptr))
                 .SIDE_EFFECT(*_2 = static_cast<std::uint32_t>(queues.size())));
        if (!queues.empty())
        {
            keep(NAMED_REQUIRE_CALL(
                     api,
                     vk_get_physical_device_queue_family_properties(
                         device, trompeloeil::_, trompeloeil::_))
                     .WITH(_3 != nullptr)
                     .SIDE_EFFECT(std::ranges::copy(queues, _3)));
        }
        keep(NAMED_REQUIRE_CALL(
                 api,
                 vk_enumerate_device_extension_properties(device, nullptr, trompeloeil::_, nullptr))
                 .SIDE_EFFECT(*_3 = static_cast<std::uint32_t>(extensions.size()))
                 .RETURN(status));
        if (status == VK_SUCCESS && !extensions.empty())
        {
            keep(NAMED_REQUIRE_CALL(
                     api,
                     vk_enumerate_device_extension_properties(
                         device, nullptr, trompeloeil::_, trompeloeil::_))
                     .WITH(_4 != nullptr)
                     .SIDE_EFFECT(std::ranges::copy(extensions, _4))
                     .RETURN(VK_SUCCESS));
        }
    }
};

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

auto name_bytes(char const * name) -> text::String
{
    return text::String{name};
}
}  // namespace

TEST_CASE("[impure] headless selection is lazy")
{
    SelectionFixture fixture;
    SelectionEnvironment environment{std::ref(fixture.api)};

    auto plan =
        reader_io_select_headless_physical_device<decltype(environment)>(fixture.instance, 0);
    [[maybe_unused]] auto deferred = plan.build(environment);

    // No query expectations exist; the separately held parent survives discarded plans.
    CHECK(fixture.instance != nullptr);
}

TEST_CASE("[impure] headless selection distinguishes query failure from no candidates")
{
    SelectionFixture fixture;
    SelectionEnvironment environment{std::ref(fixture.api)};
    VkResult status = VK_SUCCESS;
    SUBCASE("empty successful enumeration") {}
    SUBCASE("enumeration failure")
    {
        status = VK_ERROR_INITIALIZATION_FAILED;
    }
    fixture.devices({}, status);

    physical_device_selection_result result =
        reader_io_select_headless_physical_device<decltype(environment)>(fixture.instance, 0)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(result));
    if (status == VK_SUCCESS)
    {
        CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(fnfelt::result::get_error(result)));
    }
    else
    {
        REQUIRE(std::holds_alternative<VulkanOperationFailure>(fnfelt::result::get_error(result)));
        VulkanOperationFailure error =
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(result));
        CHECK(error.cause.operation == FallibleVulkanOperation::enumerate_physical_devices);
        CHECK_EQ(error.cause.result, status);
    }
}

TEST_CASE("[impure] headless selection requires Vulkan 1.1 before choosing the best device")
{
    SelectionFixture fixture;
    SelectionEnvironment environment{std::ref(fixture.api)};
    VkPhysicalDevice_T first, second, third;
    api_version second_version{VK_API_VERSION_1_1};
    api_version third_version{VK_API_VERSION_1_1};
    VkPhysicalDevice expected = &third;
    SUBCASE("minimum supported version") {}
    SUBCASE("newer API and patch revision")
    {
        third_version = api_version{VK_MAKE_API_VERSION(0, 1, 4, 318)};
    }
    SUBCASE("all devices are too old")
    {
        second_version = api_version{VK_API_VERSION_1_0};
        third_version = api_version{VK_API_VERSION_1_0};
        expected = VK_NULL_HANDLE;
    }
    fixture.devices({&first, &second, &third});
    // An earlier discrete GPU is ineligible even with a high Vulkan 1.0 patch revision.
    fixture.probe(
        &first,
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
        {queue(VK_QUEUE_GRAPHICS_BIT)},
        {},
        VK_SUCCESS,
        api_version{VK_MAKE_API_VERSION(0, 1, 0, 1023)});
    fixture.probe(
        &second,
        VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
        {queue(VK_QUEUE_GRAPHICS_BIT)},
        {},
        VK_SUCCESS,
        second_version);
    fixture.probe(
        &third,
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
        {queue(VK_QUEUE_TRANSFER_BIT), queue(VK_QUEUE_GRAPHICS_BIT)},
        {},
        VK_SUCCESS,
        third_version);

    physical_device_selection_result outcome =
        reader_io_select_headless_physical_device<decltype(environment)>(
            fixture.instance, VK_QUEUE_GRAPHICS_BIT)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    if (expected == VK_NULL_HANDLE)
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(fnfelt::result::get_error(outcome)));
    }
    else
    {
        REQUIRE(fnfelt::result::is_success(outcome));
        CHECK(fnfelt::result::get_success(outcome).physical_device == expected);
        CHECK(fnfelt::result::get_success(outcome).queue_family_index == queue_family_index{1});
    }
}

TEST_CASE("[impure] headless selection preserves handle and queue indices after filtering")
{
    SelectionFixture fixture;
    SelectionEnvironment environment{std::ref(fixture.api)};
    VkPhysicalDevice_T first, second, third;
    fixture.devices({&first, &second, &third});
    fixture.probe(
        &first,
        VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
        {queue(VK_QUEUE_TRANSFER_BIT)},
        {extension("VK_required")});
    fixture.probe(&second, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, {queue(VK_QUEUE_COMPUTE_BIT)});
    fixture.probe(
        &third,
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
        {queue(VK_QUEUE_GRAPHICS_BIT), queue(VK_QUEUE_TRANSFER_BIT)},
        {extension("VK_required")});

    physical_device_selection_result result =
        reader_io_select_headless_physical_device<decltype(environment)>(
            fixture.instance, VK_QUEUE_TRANSFER_BIT, {name_bytes("VK_required")})
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));
    CHECK(fnfelt::result::get_success(result).physical_device == &third);
    CHECK_EQ(fnfelt::result::get_success(result).queue_family_index, queue_family_index{1});
    CHECK(fnfelt::result::get_success(result).instance == fixture.instance);
}

TEST_CASE("[impure] headless selection requires complete queue and extension requirements")
{
    SelectionFixture fixture;
    SelectionEnvironment environment{std::ref(fixture.api)};
    VkPhysicalDevice_T device;
    VkQueueFlags flags{VK_QUEUE_GRAPHICS_BIT};
    immer::array<VkExtensionProperties> available{extension("VK_required_extra")};
    immer::array<text::String> required{name_bytes("VK_required")};
    SUBCASE("extension prefixes do not match") {}
    SUBCASE("every required extension must be present")
    {
        available = {extension("VK_required")};
        required = {name_bytes("VK_required"), name_bytes("VK_second_required")};
    }
    SUBCASE("missing queue capability")
    {
        flags = VK_QUEUE_TRANSFER_BIT;
        available = {extension("VK_required")};
    }
    fixture.devices({&device});
    fixture.probe(&device, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, {queue(flags)}, available);

    physical_device_selection_result result =
        reader_io_select_headless_physical_device<decltype(environment)>(
            fixture.instance, VK_QUEUE_GRAPHICS_BIT, required)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(result));
    CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(fnfelt::result::get_error(result)));
}

TEST_CASE("[impure] headless selection reports query errors in enumeration order")
{
    SelectionFixture fixture;
    SelectionEnvironment environment{std::ref(fixture.api)};
    VkPhysicalDevice_T first, second;
    VkResult second_status = VK_ERROR_INITIALIZATION_FAILED;
    SUBCASE("another query fails") {}
    SUBCASE("another device is suitable")
    {
        second_status = VK_SUCCESS;
    }
    fixture.devices({&first, &second});
    fixture.probe(
        &first,
        VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
        {queue(0)},
        {},
        VK_ERROR_OUT_OF_HOST_MEMORY);
    fixture.probe(&second, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, {queue(0)}, {}, second_status);

    physical_device_selection_result result =
        reader_io_select_headless_physical_device<decltype(environment)>(fixture.instance, 0)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(result));
    REQUIRE(std::holds_alternative<VulkanOperationFailure>(fnfelt::result::get_error(result)));
    VulkanOperationFailure error =
        std::get<VulkanOperationFailure>(fnfelt::result::get_error(result));
    CHECK(error.cause.operation == FallibleVulkanOperation::enumerate_device_extensions);
    CHECK_EQ(error.cause.result, VK_ERROR_OUT_OF_HOST_MEMORY);
}

TEST_CASE("[impure] headless selection reuses its plan with each execution API")
{
    SelectionFixture first_fixture;
    SelectionFixture second_fixture;
    SelectionEnvironment first_environment{std::ref(first_fixture.api)};
    SelectionEnvironment second_environment{std::ref(second_fixture.api)};
    VkPhysicalDevice_T first_device, second_device;
    bool has_second_extension = true;
    SUBCASE("both executions satisfy the retained requirements") {}
    SUBCASE("the second execution must still check the retained requirements")
    {
        has_second_extension = false;
    }
    first_fixture.devices({&first_device});
    first_fixture.probe(
        &first_device,
        VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
        {queue(0)},
        {extension("VK_required")});
    second_fixture.keep(
        NAMED_REQUIRE_CALL(
            second_fixture.api,
            vk_enumerate_physical_devices(&first_fixture.handle, trompeloeil::_, nullptr))
            .SIDE_EFFECT(*_2 = 1)
            .RETURN(VK_SUCCESS));
    second_fixture.keep(
        NAMED_REQUIRE_CALL(
            second_fixture.api,
            vk_enumerate_physical_devices(&first_fixture.handle, trompeloeil::_, trompeloeil::_))
            .WITH(_3 != nullptr)
            .LR_SIDE_EFFECT(_3[0] = &second_device)
            .RETURN(VK_SUCCESS));
    second_fixture.probe(
        &second_device,
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
        {queue(0)},
        has_second_extension ? immer::array{extension("VK_required")}
                             : immer::array<VkExtensionProperties>{});
    auto plan = reader_io_select_headless_physical_device<decltype(first_environment)>(
        first_fixture.instance, 0, {name_bytes("VK_required")});

    // An lvalue execution must leave the bound requirements intact for a consuming execution.
    physical_device_selection_result first =
        plan.build(first_environment).sync_wait(lf::lazy_pool{2});
    physical_device_selection_result second =
        std::move(plan).build(second_environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(first));
    CHECK(fnfelt::result::get_success(first).physical_device == &first_device);
    if (has_second_extension)
    {
        REQUIRE(fnfelt::result::is_success(second));
        CHECK(fnfelt::result::get_success(second).physical_device == &second_device);
        CHECK(
            fnfelt::result::get_success(first).instance ==
            fnfelt::result::get_success(second).instance);
    }
    else
    {
        REQUIRE(fnfelt::result::is_error(second));
        CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(fnfelt::result::get_error(second)));
    }
}

TEST_CASE("[impure] callers can recover selection by relaxing an optional extension")
{
    SelectionFixture fixture;
    SelectionEnvironment environment{std::ref(fixture.api)};
    VkPhysicalDevice_T device;
    bool has_required_extension = false;
    bool has_required_queue = true;
    SUBCASE("fallback finds a suitable device") {}
    SUBCASE("successful selection bypasses recovery")
    {
        has_required_extension = true;
    }
    SUBCASE("fallback also fails its requirements")
    {
        has_required_queue = false;
    }
    immer::array<VkQueueFamilyProperties> queues{
        queue(has_required_queue ? VK_QUEUE_GRAPHICS_BIT : VK_QUEUE_COMPUTE_BIT)};
    immer::array<VkExtensionProperties> extensions = has_required_extension
        ? immer::array<VkExtensionProperties>{extension("VK_optional")}
        : immer::array<VkExtensionProperties>{};
    fixture.devices({&device});
    fixture.probe(&device, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, queues, extensions);
    if (!has_required_extension)
    {
        // Recovery reruns discovery with the same API but without the optional requirement.
        fixture.devices({&device});
        fixture.probe(&device, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, queues, extensions);
    }
    int recovery_calls = 0;
    auto plan = reader_io_select_headless_physical_device<decltype(environment)>(
                    fixture.instance, VK_QUEUE_GRAPHICS_BIT, {name_bytes("VK_optional")})
                    .or_else(
                        [&](renderer_error error)
                        {
                            ++recovery_calls;
                            CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(error));
                            return reader_io_select_headless_physical_device<decltype(environment)>(
                                fixture.instance, VK_QUEUE_GRAPHICS_BIT);
                        });
    auto deferred = plan.build(environment);

    CHECK_EQ(recovery_calls, 0);

    physical_device_selection_result result = std::move(deferred).sync_wait(lf::lazy_pool{2});

    CHECK_EQ(recovery_calls, has_required_extension ? 0 : 1);
    if (has_required_queue)
    {
        REQUIRE(fnfelt::result::is_success(result));
        CHECK(fnfelt::result::get_success(result).physical_device == &device);
        CHECK(fnfelt::result::get_success(result).instance == fixture.instance);
    }
    else
    {
        REQUIRE(fnfelt::result::is_error(result));
        CHECK(std::holds_alternative<NoSuitablePhysicalDevice>(fnfelt::result::get_error(result)));
    }
}

TEST_CASE("[impure] stopped or failed acquisition bypasses device discovery")
{
    SelectionFixture fixture;
    SelectionEnvironment environment{std::ref(fixture.api)};
    using acquisition_result = renderer_result<instance_ptr>;
    acquisition_result acquisition = fnfelt::result::stopped<acquisition_result>();
    bool has_failed = false;
    SUBCASE("cooperative stop") {}
    SUBCASE("acquisition error")
    {
        acquisition = fnfelt::result::error<acquisition_result>(renderer_error_from_vulkan_failure(
            VulkanFailure{
                FallibleVulkanOperation::create_instance, VK_ERROR_INITIALIZATION_FAILED}));
        has_failed = true;
    }
    int error_mapping_calls = 0;
    auto plan = fnfelt::monad::reader_io::just<decltype(environment)>(acquisition)
                    .and_then(
                        [](instance_ptr instance)
                        {
                            return reader_io_select_headless_physical_device<decltype(environment)>(
                                std::move(instance), 0);
                        })
                    .transform_error(
                        [&](renderer_error error)
                        {
                            ++error_mapping_calls;
                            return fnfelt::result::error<physical_device_selection_result>(error);
                        });

    // There are no discovery expectations: either non-success must bypass all Vulkan queries.
    physical_device_selection_result result = plan.build(environment).sync_wait(lf::lazy_pool{2});

    CHECK_EQ(error_mapping_calls, has_failed ? 1 : 0);
    if (has_failed)
    {
        REQUIRE(fnfelt::result::is_error(result));
        CHECK_EQ(
            std::get<VulkanOperationFailure>(fnfelt::result::get_error(result)).cause.result,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    else
    {
        CHECK(fnfelt::result::is_stopped(result));
    }
}

TEST_CASE("[impure] headless selection owns requirements and returned parent")
{
    SelectionFixture fixture;
    SelectionEnvironment environment{std::ref(fixture.api)};
    VkPhysicalDevice_T device;
    fixture.devices({&device});
    fixture.probe(
        &device, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, {queue(0)}, {extension("VK_required")});
    immer::array<text::String> required{name_bytes("VK_required")};
    std::optional<SelectedPhysicalDevice> selected;

    {
        auto plan = reader_io_select_headless_physical_device<decltype(environment)>(
            fixture.instance, 0, required);
        required = {};
        physical_device_selection_result result =
            plan.build(environment).sync_wait(lf::lazy_pool{2});
        REQUIRE(fnfelt::result::is_success(result));
        selected = fnfelt::result::get_success(result);
    }
    fixture.instance.reset();

    REQUIRE(selected.has_value());
    CHECK(selected->instance.get() == &fixture.handle);
    CHECK(selected->physical_device == &device);
    // The selected value releases the final parent while cleanup expectations are still alive.
    selected.reset();
}

TEST_CASE("[integration][headless] headless selection discovers a usable device")
{
    backend::Api api;
    SelectionEnvironment environment{std::ref(api)};
    renderer_result<instance_ptr> instance = reader_io_vk_create_instance<decltype(environment)>()
                                                 .build(environment)
                                                 .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(instance));

    physical_device_selection_result result =
        reader_io_select_headless_physical_device<decltype(environment)>(
            fnfelt::result::get_success(instance), 0)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));
    CHECK(fnfelt::result::get_success(result).instance == fnfelt::result::get_success(instance));
    CHECK(fnfelt::result::get_success(result).physical_device != VK_NULL_HANDLE);
    immer::array<VkQueueFamilyProperties> queues =
        reader_io_vk_get_physical_device_queue_family_properties<decltype(environment)>(
            fnfelt::result::get_success(result).physical_device)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});
    REQUIRE(value_of(fnfelt::result::get_success(result).queue_family_index) < queues.size());
    CHECK_GT(
        queues[value_of(fnfelt::result::get_success(result).queue_family_index)].queueCount, 0);
}
}  // namespace vulkandemo::vulkan

namespace vulkandemo::backend
{
namespace
{
template <class TApi>
struct ApiEnvironment
{
    std::reference_wrapper<TApi> api;
};

struct DeviceQueryApi
{
    VkResult vk_enumerate_physical_devices(VkInstance, std::uint32_t *, VkPhysicalDevice *);
    void vk_get_physical_device_properties(VkPhysicalDevice, VkPhysicalDeviceProperties *);
    void vk_get_physical_device_queue_family_properties(
        VkPhysicalDevice, std::uint32_t *, VkQueueFamilyProperties *);
    VkResult vk_enumerate_device_extension_properties(
        VkPhysicalDevice, char const *, std::uint32_t *, VkExtensionProperties *);
};

static_assert(
    vulkan::detail::select_headless_physical_device_environment<ApiEnvironment<DeviceQueryApi>>);
}  // namespace
}  // namespace vulkandemo::backend
#endif  // DOCTEST_CONFIG_DISABLE
