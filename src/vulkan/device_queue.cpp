// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./device_queue.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./device.hpp"
#include "./instance.hpp"
#include "./physical_device_selection.hpp"
#include "./strong_types.hpp"

struct VkInstance_T
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
static_assert(!std::is_convertible_v<std::uint32_t, queue_index>);
static_assert(!std::is_convertible_v<queue_index, std::uint32_t>);
static_assert(!std::is_convertible_v<queue_family_index, queue_index>);
static_assert(!std::is_convertible_v<queue_index, queue_family_index>);

template <class TApi>
struct QueueEnvironment
{
    std::reference_wrapper<TApi> api;
};
}  // namespace

TEST_CASE("[impure] device queue plans are lazy and retain their device")
{
    backend::MockApi api;
    QueueEnvironment environment{std::ref(api)};
    VkInstance_T instance;
    VkDevice_T device;
    bool is_device_destroyed = false;
    REQUIRE_CALL(api, vk_destroy_device(&device, nullptr))
        .LR_SIDE_EFFECT(is_device_destroyed = true);
    REQUIRE_CALL(api, vk_destroy_instance(&instance, nullptr))
        .LR_SIDE_EFFECT(CHECK(is_device_destroyed));
    auto owner = make_device_ptr(&device, make_instance_ptr(&instance, api), api);

    {
        auto plan = reader_io_vk_get_device_queue<decltype(environment)>(
            owner, queue_family_index{4}, queue_index{2});
        [[maybe_unused]] auto deferred = plan.build(environment);
        owner.reset();

        // Neither building nor discarding an operation queries the API.
        CHECK_FALSE(is_device_destroyed);
    }

    // The discarded plans release their retained device before its instance.
    CHECK(is_device_destroyed);
}

TEST_CASE("[impure] device queue queries forward indices and use the supplied API on each run")
{
    backend::MockApi first_api;
    backend::MockApi second_api;
    QueueEnvironment first_environment{std::ref(first_api)};
    QueueEnvironment second_environment{std::ref(second_api)};
    VkInstance_T instance;
    VkDevice_T device;
    VkQueue_T first_queue;
    VkQueue_T second_queue;
    REQUIRE_CALL(first_api, vk_destroy_device(&device, nullptr));
    REQUIRE_CALL(first_api, vk_destroy_instance(&instance, nullptr));
    REQUIRE_CALL(first_api, vk_get_device_queue(&device, 4, 2, trompeloeil::_))
        .WITH(_4 != nullptr)
        .LR_SIDE_EFFECT(*_4 = &first_queue);
    REQUIRE_CALL(second_api, vk_get_device_queue(&device, 4, 2, trompeloeil::_))
        .WITH(_4 != nullptr)
        .LR_SIDE_EFFECT(*_4 = &second_queue);
    auto owner = make_device_ptr(&device, make_instance_ptr(&instance, first_api), first_api);
    auto plan = reader_io_vk_get_device_queue<decltype(first_environment)>(
        owner, queue_family_index{4}, queue_index{2});

    auto first = plan.build(first_environment).sync_wait(lf::lazy_pool{2});
    auto second = plan.build(second_environment).sync_wait(lf::lazy_pool{2});

    CHECK(first == &first_queue);
    CHECK(second == &second_queue);
    CHECK(owner.get() == &device);
}

TEST_CASE("[impure] built device queue IO retains its device after the source plan is discarded")
{
    backend::MockApi api;
    QueueEnvironment environment{std::ref(api)};
    VkInstance_T instance;
    VkDevice_T device;
    VkQueue_T queue;
    bool is_device_destroyed = false;
    REQUIRE_CALL(api, vk_destroy_device(&device, nullptr))
        .LR_SIDE_EFFECT(is_device_destroyed = true);
    REQUIRE_CALL(api, vk_destroy_instance(&instance, nullptr))
        .LR_SIDE_EFFECT(CHECK(is_device_destroyed));
    REQUIRE_CALL(api, vk_get_device_queue(&device, 3, 0, trompeloeil::_))
        .LR_SIDE_EFFECT(CHECK_FALSE(is_device_destroyed))
        .LR_SIDE_EFFECT(*_4 = &queue);
    auto owner = make_device_ptr(&device, make_instance_ptr(&instance, api), api);
    std::weak_ptr<VkDevice_T> weak = owner;

    {
        auto deferred = [&]
        {
            auto plan = reader_io_vk_get_device_queue<decltype(environment)>(
                owner, queue_family_index{3}, queue_index{0});
            return plan.build(environment).io();
        }();
        owner.reset();

        auto borrowed_queue = deferred.build().sync_wait(lf::lazy_pool{2});

        CHECK(borrowed_queue == &queue);
        CHECK_FALSE(is_device_destroyed);
        CHECK_FALSE(weak.expired());
    }

    // The raw queue carries no ownership; the deferred operation supplied its lifetime here.
    CHECK(is_device_destroyed);
    CHECK(weak.expired());
}

TEST_CASE("[integration][headless] a logical device queue can be retrieved")
{
    backend::Api api;
    QueueEnvironment environment{std::ref(api)};
    auto instance = reader_io_vk_create_instance<decltype(environment)>()
                        .build(environment)
                        .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(instance));
    auto selection = reader_io_select_headless_physical_device<decltype(environment)>(
                         fnfelt::result::get_success(instance), VK_QUEUE_GRAPHICS_BIT)
                         .build(environment)
                         .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(selection));
    auto const & selected = fnfelt::result::get_success(selection);
    auto device = reader_io_vk_create_device<decltype(environment)>(
                      selected.instance, selected.physical_device, selected.queue_family_index)
                      .build(environment)
                      .sync_wait(lf::lazy_pool{2});
    REQUIRE(fnfelt::result::is_success(device));

    auto queue =
        reader_io_vk_get_device_queue<decltype(environment)>(
            fnfelt::result::get_success(device), selected.queue_family_index, queue_index{0})
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    CHECK(queue != VK_NULL_HANDLE);
    // No queue work was submitted, so the retained device can be released at scope exit.
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
