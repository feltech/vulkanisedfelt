// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./image_format_properties.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/mock_api.hpp"
#include "./errors.hpp"

namespace vulkandemo::vulkan
{
namespace
{
struct ImageFormatEnvironment
{
    std::reference_wrapper<backend::MockApi> api;
};
}  // namespace

TEST_CASE("[impure] image-format query plans are lazy and preserve exact parameters across replay")
{
    backend::MockApi api;
    ImageFormatEnvironment environment{std::ref(api)};
    char storage{};
    VkPhysicalDevice device = reinterpret_cast<VkPhysicalDevice>(&storage);
    VkImageFormatProperties expected{};
    expected.maxExtent = {4096, 2048, 1};
    expected.maxMipLevels = 12;
    expected.maxArrayLayers = 256;
    expected.sampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;
    expected.maxResourceSize = 1048576;
    auto plan = reader_io_vk_get_physical_device_image_format_properties<ImageFormatEnvironment>(
        device,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        0);
    [[maybe_unused]] auto discarded = plan.build(environment);
    REQUIRE_CALL(
        api,
        vk_get_physical_device_image_format_properties(
            device,
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            0,
            trompeloeil::_))
        .LR_SIDE_EFFECT(*_7 = expected)
        .LR_SIDE_EFFECT(++expected.maxExtent.width)
        .RETURN(VK_SUCCESS)
        .TIMES(2);

    vulkan_result<VkImageFormatProperties> first =
        plan.build(environment).sync_wait(lf::lazy_pool{2});
    vulkan_result<VkImageFormatProperties> second =
        plan.build(environment).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(first));
    REQUIRE(fnfelt::result::is_success(second));
    VkImageFormatProperties properties = fnfelt::result::get_success(first);
    CHECK_EQ(properties.maxExtent.width, 4096U);
    CHECK_EQ(properties.maxExtent.height, 2048U);
    CHECK_EQ(properties.maxExtent.depth, 1U);
    CHECK_EQ(properties.maxMipLevels, 12U);
    CHECK_EQ(properties.maxArrayLayers, 256U);
    CHECK_EQ(properties.sampleCounts, VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT);
    CHECK_EQ(properties.maxResourceSize, 1048576U);
    CHECK_EQ(fnfelt::result::get_success(second).maxExtent.width, 4097U);
}

TEST_CASE("[impure] image-format queries preserve unsupported and operational errors")
{
    backend::MockApi api;
    ImageFormatEnvironment environment{std::ref(api)};
    char storage{};
    VkPhysicalDevice device = reinterpret_cast<VkPhysicalDevice>(&storage);
    VkResult status = VK_ERROR_FORMAT_NOT_SUPPORTED;
    SUBCASE("unsupported format") {}
    SUBCASE("host allocation failure")
    {
        status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    REQUIRE_CALL(
        api,
        vk_get_physical_device_image_format_properties(
            device,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            0,
            trompeloeil::_))
        .LR_RETURN(status);

    vulkan_result<VkImageFormatProperties> outcome =
        reader_io_vk_get_physical_device_image_format_properties<ImageFormatEnvironment>(
            device,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            0)
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK(
        fnfelt::result::get_error(outcome).operation ==
        FallibleVulkanOperation::get_physical_device_image_format_properties);
    CHECK_EQ(fnfelt::result::get_error(outcome).result, status);
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
