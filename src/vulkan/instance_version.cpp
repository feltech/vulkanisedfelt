// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./instance_version.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./api_version.hpp"

namespace vulkandemo::vulkan
{
namespace
{
template <class TApi>
struct VersionEnvironment
{
    std::reference_wrapper<TApi> api;
};

VKAPI_ATTR VkResult VKAPI_CALL enumerate_version_1_1(std::uint32_t * version)
{
    *version = VK_API_VERSION_1_1;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL enumerate_version_1_4(std::uint32_t * version)
{
    *version = VK_MAKE_API_VERSION(0, 1, 4, 318);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL fail_version_query(std::uint32_t *)
{
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}
}  // namespace

TEST_CASE("[impure] instance version discovery is lazy and preserves reported revisions")
{
    backend::MockApi api;
    VersionEnvironment environment{std::ref(api)};
    api_version expected{VK_API_VERSION_1_1};
    PFN_vkEnumerateInstanceVersion query = &enumerate_version_1_1;
    SUBCASE("Vulkan 1.1") {}
    SUBCASE("newer implementation with patch revision")
    {
        expected = api_version{VK_MAKE_API_VERSION(0, 1, 4, 318)};
        query = &enumerate_version_1_4;
    }
    auto plan = reader_io_vk_enumerate_instance_version<decltype(environment)>();
    auto deferred = plan.build(environment);

    REQUIRE_CALL(api, vk_get_instance_proc_addr(nullptr, trompeloeil::_))
        .WITH(std::strcmp(_2, "vkEnumerateInstanceVersion") == 0)
        .LR_RETURN(reinterpret_cast<PFN_vkVoidFunction>(query));

    vulkan_result<api_version> outcome = std::move(deferred).sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK(fnfelt::result::get_success(outcome) == expected);
}

TEST_CASE("[impure] missing instance version query identifies a Vulkan 1.0 implementation")
{
    backend::MockApi api;
    VersionEnvironment environment{std::ref(api)};
    REQUIRE_CALL(api, vk_get_instance_proc_addr(nullptr, trompeloeil::_))
        .WITH(std::strcmp(_2, "vkEnumerateInstanceVersion") == 0)
        .RETURN(nullptr);

    vulkan_result<api_version> outcome =
        reader_io_vk_enumerate_instance_version<decltype(environment)>()
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK(fnfelt::result::get_success(outcome) == api_version{VK_API_VERSION_1_0});
}

TEST_CASE("[impure] instance version discovery preserves native query failures")
{
    backend::MockApi api;
    VersionEnvironment environment{std::ref(api)};
    REQUIRE_CALL(api, vk_get_instance_proc_addr(nullptr, trompeloeil::_))
        .WITH(std::strcmp(_2, "vkEnumerateInstanceVersion") == 0)
        .RETURN(reinterpret_cast<PFN_vkVoidFunction>(&fail_version_query));

    vulkan_result<api_version> outcome =
        reader_io_vk_enumerate_instance_version<decltype(environment)>()
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_error(outcome));
    CHECK(
        fnfelt::result::get_error(outcome).operation ==
        FallibleVulkanOperation::enumerate_instance_version);
    CHECK(fnfelt::result::get_error(outcome).result == VK_ERROR_OUT_OF_HOST_MEMORY);
}

TEST_CASE("[integration][headless] installed Vulkan implementation supports the instance baseline")
{
    backend::Api api;
    VersionEnvironment environment{std::ref(api)};

    vulkan_result<api_version> outcome =
        reader_io_vk_enumerate_instance_version<decltype(environment)>()
            .build(environment)
            .sync_wait(lf::lazy_pool{2});

    REQUIRE(fnfelt::result::is_success(outcome));
    CHECK(supports_api_version(fnfelt::result::get_success(outcome)));
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
