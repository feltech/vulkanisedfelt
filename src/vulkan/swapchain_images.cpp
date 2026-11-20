// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain_images.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <ranges>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/mock_api.hpp"
#include "./errors.hpp"
#include "./swapchain_images_types.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct ImageQueryEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] swapchain image plans are lazy and retain their inputs")
{
    backend::MockApi api;
    ImageQueryEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    swapchain_ptr swapchain{reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}};
    std::weak_ptr device_observer{device};
    std::weak_ptr swapchain_observer{swapchain};

    {
        // Building a deferred operation must retain parents without querying the API.
        auto plan = reader_io_vk_get_swapchain_images_khr<ImageQueryEnvironment>(
            std::move(device), std::move(swapchain));
        [[maybe_unused]] auto deferred = plan.build(environment);

        CHECK_FALSE(device_observer.expired());
        CHECK_FALSE(swapchain_observer.expired());
    }

    CHECK(device_observer.expired());
    CHECK(swapchain_observer.expired());
}

TEST_CASE("[impure] swapchain image snapshots retain parents and preserve written order")
{
    backend::MockApi api;
    ImageQueryEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{}, first_storage{}, second_storage{};
    VkDevice device_handle = reinterpret_cast<VkDevice>(&device_storage);
    VkSwapchainKHR swapchain_handle = reinterpret_cast<VkSwapchainKHR>(&swapchain_storage);
    int cleanup_step = 0;
    device_ptr device{device_handle, [&](auto) { CHECK_EQ(++cleanup_step, 2); }};
    swapchain_ptr swapchain{
        swapchain_handle,
        [parent = device, &cleanup_step](auto) mutable
        {
            CHECK_EQ(++cleanup_step, 1);
            parent.reset();
        }};
    immer::array<VkImage> expected{
        reinterpret_cast<VkImage>(&first_storage), reinterpret_cast<VkImage>(&second_storage)};
    std::uint32_t written = 2;
    SUBCASE("exact fill") {}
    SUBCASE("short fill")
    {
        written = 1;
    }
    SUBCASE("empty fill")
    {
        written = 0;
    }
    trompeloeil::sequence calls;
    REQUIRE_CALL(
        api, vk_get_swapchain_images_khr(device_handle, swapchain_handle, trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 2)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);
    REQUIRE_CALL(
        api,
        vk_get_swapchain_images_khr(
            device_handle, swapchain_handle, trompeloeil::_, trompeloeil::_))
        .WITH(_4 != nullptr && *_3 == 2)
        .LR_SIDE_EFFECT(std::ranges::copy(std::views::take(expected, written), _4))
        .LR_SIDE_EFFECT(*_3 = written)
        .RETURN(VK_SUCCESS)
        .IN_SEQUENCE(calls);

    // The temporary plan and all input owners disappear before checking output ownership.
    vulkan_result<SwapchainImages> outcome =
        reader_io_vk_get_swapchain_images_khr<ImageQueryEnvironment>(
            std::move(device), std::move(swapchain))
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    SwapchainImages snapshot = fnfelt::result::get_success(std::move(outcome));
    CHECK_EQ(snapshot.swapchain.get(), swapchain_handle);
    CHECK(std::ranges::equal(snapshot.images, std::views::take(expected, written)));
    CHECK_EQ(cleanup_step, 0);
    snapshot.swapchain.reset();
    CHECK_EQ(cleanup_step, 2);
}

TEST_CASE("[impure] swapchain image count queries can return an empty snapshot")
{
    backend::MockApi api;
    ImageQueryEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    swapchain_ptr swapchain{reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}};
    REQUIRE_CALL(
        api, vk_get_swapchain_images_khr(device.get(), swapchain.get(), trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 0)
        .RETURN(VK_SUCCESS);

    vulkan_result<SwapchainImages> outcome =
        reader_io_vk_get_swapchain_images_khr<ImageQueryEnvironment>(device, swapchain)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK(fnfelt::result::get_success(outcome).images.empty());
    CHECK(fnfelt::result::get_success(outcome).swapchain == swapchain);
}

TEST_CASE("[impure] swapchain image queries retry incomplete snapshots without exposing partials")
{
    backend::MockApi api;
    ImageQueryEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{}, image_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    swapchain_ptr swapchain{reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}};
    VkImage image = reinterpret_cast<VkImage>(&image_storage);
    bool is_count_incomplete = false;
    bool is_exhausted = false;
    SUBCASE("count then success")
    {
        is_count_incomplete = true;
    }
    SUBCASE("fill then success") {}
    SUBCASE("count exhaustion")
    {
        is_count_incomplete = true;
        is_exhausted = true;
    }
    SUBCASE("fill exhaustion")
    {
        is_exhausted = true;
    }
    int attempts = 0;
    REQUIRE_CALL(
        api, vk_get_swapchain_images_khr(device.get(), swapchain.get(), trompeloeil::_, nullptr))
        .LR_SIDE_EFFECT(++attempts)
        .SIDE_EFFECT(*_3 = 1)
        .LR_RETURN(
            is_count_incomplete && (is_exhausted || attempts == 1) ? VK_INCOMPLETE : VK_SUCCESS)
        .TIMES(2, 3);
    auto fill = NAMED_ALLOW_CALL(
                    api,
                    vk_get_swapchain_images_khr(
                        device.get(), swapchain.get(), trompeloeil::_, trompeloeil::_))
                    .WITH(_4 != nullptr && *_3 == 1)
                    .LR_SIDE_EFFECT(*_4 = image)
                    .LR_RETURN(
                        !is_count_incomplete && (is_exhausted || attempts == 1) ? VK_INCOMPLETE
                                                                                : VK_SUCCESS);

    vulkan_result<SwapchainImages> outcome =
        reader_io_vk_get_swapchain_images_khr<ImageQueryEnvironment>(device, swapchain)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    CHECK_EQ(attempts, is_exhausted ? 3 : 2);
    if (is_exhausted)
    {
        REQUIRE(fnfelt::result::is_error(outcome));
        CHECK(fnfelt::result::get_error(outcome).result == VK_INCOMPLETE);
    }
    else
    {
        REQUIRE(fnfelt::result::is_success(outcome));
        REQUIRE_EQ(fnfelt::result::get_success(outcome).images.size(), 1U);
        CHECK_EQ(fnfelt::result::get_success(outcome).images[0], image);
    }
}

TEST_CASE("[impure] swapchain image queries preserve count and fill errors")
{
    backend::MockApi api;
    ImageQueryEnvironment environment{std::ref(api)};
    char device_storage{}, swapchain_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    swapchain_ptr swapchain{reinterpret_cast<VkSwapchainKHR>(&swapchain_storage), [](auto) {}};
    VkResult count_status = VK_ERROR_OUT_OF_HOST_MEMORY;
    std::unique_ptr<trompeloeil::expectation> fill_error;
    SUBCASE("count error") {}
    SUBCASE("fill error")
    {
        count_status = VK_SUCCESS;
        fill_error = NAMED_REQUIRE_CALL(
                         api,
                         vk_get_swapchain_images_khr(
                             device.get(), swapchain.get(), trompeloeil::_, trompeloeil::_))
                         .WITH(_4 != nullptr)
                         .RETURN(VK_ERROR_OUT_OF_HOST_MEMORY);
    }
    REQUIRE_CALL(
        api, vk_get_swapchain_images_khr(device.get(), swapchain.get(), trompeloeil::_, nullptr))
        .SIDE_EFFECT(*_3 = 1)
        .LR_RETURN(count_status);

    vulkan_result<SwapchainImages> outcome =
        reader_io_vk_get_swapchain_images_khr<ImageQueryEnvironment>(device, swapchain)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK(
        fnfelt::result::get_error(outcome).operation ==
        FallibleVulkanOperation::get_swapchain_images);
    CHECK(fnfelt::result::get_error(outcome).result == VK_ERROR_OUT_OF_HOST_MEMORY);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
