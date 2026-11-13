// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./instance.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"

// Complete the opaque dispatchable handle only for test-owned fake instances.
struct VkInstance_T
{
};

namespace vulkandemo::vulkan
{
namespace
{
using backend::Api;
using backend::MockApi;

template <class TApi>
struct TestEnvironment
{
    std::reference_wrapper<TApi> api;
};

struct CustomEnvironment
{
    std::reference_wrapper<MockApi> api;
    int extra_state;
};

auto extension_name(char const * name)
{
    return text::String{name};
}
}  // namespace

TEST_CASE("[impure] vk_create_instance is lazy and destroys only the final owner")
{
    MockApi api;
    // Report unexpected cleanup without throwing through shared_ptr destruction.
    ALLOW_CALL(api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected instance destruction"));

    // Neither constructing the plan nor supplying its environment may call the API.
    TestEnvironment<MockApi> environment{std::ref(api)};
    auto plan = reader_io_vk_create_instance<TestEnvironment<MockApi>>();
    auto deferred = plan.build(environment);

    // Expect a minimal create-info and return a test-owned handle on execution.
    VkInstance_T handle;
    REQUIRE_CALL(api, vk_create_instance(trompeloeil::_, nullptr, trompeloeil::_))
        .WITH(
            _1->sType == VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO && _1->pNext == nullptr &&
            _1->flags == 0 && _1->pApplicationInfo == nullptr && _1->enabledLayerCount == 0 &&
            _1->ppEnabledLayerNames == nullptr && _1->enabledExtensionCount == 0 &&
            _1->ppEnabledExtensionNames == nullptr)
        .LR_SIDE_EFFECT(*_3 = &handle)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_destroy_instance(&handle, nullptr));

    auto result = std::move(deferred).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(result));

    // Copy ownership, then release the original while keeping the instance alive.
    auto owner = fnfelt::result::get_success(result);

    CHECK(owner.get() == &handle);

    fnfelt::result::get_success(result).reset();

    CHECK_EQ(owner.use_count(), 1);
    // Leaving this scope releases the final owner and satisfies the destroy expectation.
}

TEST_CASE("[impure] vk_create_instance preserves failure without destruction")
{
    MockApi api;
    ALLOW_CALL(api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected instance destruction"));
    REQUIRE_CALL(api, vk_create_instance(trompeloeil::_, nullptr, trompeloeil::_))
        .RETURN(VK_ERROR_INCOMPATIBLE_DRIVER);
    TestEnvironment<MockApi> environment{std::ref(api)};
    auto result = reader_io_vk_create_instance<TestEnvironment<MockApi>>()
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_error(result));
    CHECK(fnfelt::result::get_error(result).result == VK_ERROR_INCOMPATIBLE_DRIVER);
    CHECK(fnfelt::result::get_error(result).operation == FallibleVulkanOperation::create_instance);
}

TEST_CASE("[impure] vk_create_instance owns extension names for deferred replay")
{
    MockApi api;
    ALLOW_CALL(api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected instance destruction"));
    TestEnvironment<MockApi> environment{std::ref(api)};
    VkInstance_T first;
    VkInstance_T second;
    trompeloeil::sequence calls;

    // The reusable plan retains names after the construction inputs leave scope.
    auto plan = []
    {
        auto names = immer::array<text::String>{
            extension_name("VK_test_first"), extension_name("VK_test_second_long_extension_name")};
        return reader_io_vk_create_instance<TestEnvironment<MockApi>>(std::move(names));
    }();
    auto deferred = plan.build(environment);

    // Vulkan receives terminated strings while the project-owned names contain bytes only.
    REQUIRE_CALL(api, vk_create_instance(trompeloeil::_, nullptr, trompeloeil::_))
        .WITH(
            _1->enabledExtensionCount == 2 && _1->ppEnabledExtensionNames != nullptr &&
            std::strcmp(_1->ppEnabledExtensionNames[0], "VK_test_first") == 0 &&
            std::strcmp(_1->ppEnabledExtensionNames[1], "VK_test_second_long_extension_name") == 0)
        .LR_SIDE_EFFECT(*_3 = &first)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, vk_destroy_instance(&first, nullptr)).IN_SEQUENCE(calls);
    REQUIRE_CALL(api, vk_create_instance(trompeloeil::_, nullptr, trompeloeil::_))
        .WITH(
            _1->enabledExtensionCount == 2 && _1->ppEnabledExtensionNames != nullptr &&
            std::strcmp(_1->ppEnabledExtensionNames[0], "VK_test_first") == 0 &&
            std::strcmp(_1->ppEnabledExtensionNames[1], "VK_test_second_long_extension_name") == 0)
        .LR_SIDE_EFFECT(*_3 = &second)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(api, vk_destroy_instance(&second, nullptr)).IN_SEQUENCE(calls);

    auto first_result = std::move(deferred).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(first_result));
    CHECK(fnfelt::result::get_success(first_result).get() == &first);

    fnfelt::result::get_success(first_result).reset();
    auto second_result = plan.build(environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(second_result));
    CHECK(fnfelt::result::get_success(second_result).get() == &second);
}

TEST_CASE("[impure] vk_create_instance discarded plans make no calls")
{
    MockApi api;
    TestEnvironment<MockApi> environment{std::ref(api)};
    auto plan = reader_io_vk_create_instance<TestEnvironment<MockApi>>();
    [[maybe_unused]] auto deferred = plan.build(environment);
}

TEST_CASE("[impure] vk_create_instance reuses a plan with copied environments")
{
    MockApi api;
    ALLOW_CALL(api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected instance destruction"));
    TestEnvironment<MockApi> environment{std::ref(api)};
    auto copied = environment;
    auto plan = reader_io_vk_create_instance<TestEnvironment<MockApi>>();
    VkInstance_T first;
    VkInstance_T second;
    REQUIRE_CALL(api, vk_create_instance(trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(*_3 = &second)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_create_instance(trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(*_3 = &first)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(api, vk_destroy_instance(&first, nullptr));
    REQUIRE_CALL(api, vk_destroy_instance(&second, nullptr));
    auto first_result = plan.build(environment).sync_wait(lf::lazy_pool{2});
    auto second_result = plan.build(copied).sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(first_result));
    REQUIRE(fnfelt::result::is_success(second_result));
    CHECK(fnfelt::result::get_success(first_result).get() == &first);
    CHECK(fnfelt::result::get_success(second_result).get() == &second);
}

TEST_CASE("[impure] vk_create_instance uses each execution environment")
{
    MockApi first_api;
    MockApi second_api;
    ALLOW_CALL(first_api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected first destruction"));
    ALLOW_CALL(second_api, vk_destroy_instance(trompeloeil::_, trompeloeil::_))
        .SIDE_EFFECT(DOCTEST_FAIL_CHECK("unexpected second destruction"));
    CustomEnvironment first_environment{std::ref(first_api), 10};
    CustomEnvironment second_environment{std::ref(second_api), 20};
    auto plan = reader_io_vk_create_instance<CustomEnvironment>();
    VkInstance_T first;
    VkInstance_T second;
    REQUIRE_CALL(first_api, vk_create_instance(trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(*_3 = &first)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(second_api, vk_create_instance(trompeloeil::_, nullptr, trompeloeil::_))
        .LR_SIDE_EFFECT(*_3 = &second)
        .RETURN(VK_SUCCESS);
    REQUIRE_CALL(first_api, vk_destroy_instance(&first, nullptr));
    REQUIRE_CALL(second_api, vk_destroy_instance(&second, nullptr));
    auto first_result = plan.build(first_environment).sync_wait(lf::lazy_pool{2});
    auto second_result = plan.build(second_environment).sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(first_result));
    REQUIRE(fnfelt::result::is_success(second_result));
    CHECK(fnfelt::result::get_success(first_result).get() == &first);
    CHECK(fnfelt::result::get_success(second_result).get() == &second);
}

TEST_CASE("[integration][headless] vk_create_instance creates a real instance")
{
    Api api;
    TestEnvironment<Api> environment{std::ref(api)};
    auto result = reader_io_vk_create_instance<TestEnvironment<Api>>()
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(result));
    CHECK(fnfelt::result::get_success(result).get() != VK_NULL_HANDLE);
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

struct InstanceApi
{
    VkResult vk_create_instance(
        VkInstanceCreateInfo const *, VkAllocationCallbacks const *, VkInstance *) const;
    void vk_destroy_instance(VkInstance, VkAllocationCallbacks const *) const;
};

struct CreateOnlyApi
{
    VkResult vk_create_instance(
        VkInstanceCreateInfo const *, VkAllocationCallbacks const *, VkInstance *);
};

struct WrongInstanceApi
{
    int vk_create_instance(
        VkInstanceCreateInfo const *, VkAllocationCallbacks const *, VkInstance *);
    void vk_destroy_instance(VkInstance, VkAllocationCallbacks const *);
};

struct DirectApiEnvironment
{
    InstanceApi api;
};

static_assert(vulkan::vk_create_instance_environment<ApiEnvironment<InstanceApi>>);
static_assert(vulkan::vk_create_instance_environment<ApiEnvironment<InstanceApi const> const &>);
static_assert(!vulkan::vk_create_instance_environment<ApiEnvironment<CreateOnlyApi>>);
static_assert(!vulkan::vk_create_instance_environment<ApiEnvironment<WrongInstanceApi>>);
static_assert(!vulkan::vk_create_instance_environment<int>);
static_assert(requires { vulkan::reader_io_vk_create_instance<ApiEnvironment<InstanceApi>>(); });
static_assert(!vulkan::vk_create_instance_environment<DirectApiEnvironment>);
static_assert(!vulkan::vk_create_instance_environment<ApiEnvironment<Api const>>);
}  // namespace
}  // namespace vulkandemo::backend
#endif  // DOCTEST_CONFIG_DISABLE
