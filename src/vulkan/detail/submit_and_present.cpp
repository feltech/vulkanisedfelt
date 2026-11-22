// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./submit_and_present.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <functional>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>

#include "../../backend/mock_api.hpp"
#endif  // DOCTEST_CONFIG_DISABLE

#ifndef DOCTEST_CONFIG_DISABLE
namespace vulkandemo::vulkan::detail
{
TEST_CASE("[impure] unproven frame completion invokes the terminal policy")
{
    backend::MockApi api;
    char device_storage{}, fence_storage{};
    device_ptr device{reinterpret_cast<VkDevice>(&device_storage), [](auto) {}};
    fence_ptr fence{reinterpret_cast<VkFence>(&fence_storage), [](auto) {}};
    VkResult status = VK_ERROR_OUT_OF_HOST_MEMORY;
    SUBCASE("host allocation failure") {}
    SUBCASE("device allocation failure")
    {
        status = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    SUBCASE("timeout despite an indefinite wait")
    {
        status = VK_TIMEOUT;
    }
    SUBCASE("unexpected native wait failure")
    {
        status = VK_ERROR_UNKNOWN;
    }
    REQUIRE_CALL(api, vk_wait_for_fences(device.get(), 1, trompeloeil::_, VK_TRUE, trompeloeil::_))
        .RETURN(status);
    VulkanFailure failure{FallibleVulkanOperation::wait_for_fence, status};
    REQUIRE_CALL(api, report_fatal_error_effect(trompeloeil::_))
        .WITH(_1.operation == FallibleVulkanOperation::wait_for_fence && _1.result == status)
        .THROW(failure);

    // The mock interrupts the terminal effect before a result could permit unsafe destruction.
    auto completion = io_wait_for_frame_release(std::ref(api), device, fence);
    CHECK_THROWS_AS((void)completion.build().sync_wait(), VulkanFailure);
}
}  // namespace vulkandemo::vulkan::detail
#endif  // DOCTEST_CONFIG_DISABLE
