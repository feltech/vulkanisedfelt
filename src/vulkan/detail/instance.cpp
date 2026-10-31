// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./instance.hpp"

#include <fnfelt/result.hpp>

#include "../api_version.hpp"

namespace vulkandemo::vulkan::detail
{
renderer_result<void> require_instance_api_version(api_version available)
{
    namespace result = fnfelt::result;
    using result_type = renderer_result<void>;
    if (!supports_api_version(available))
    {
        return result::error<result_type>(
            renderer_error{UnsupportedVulkanVersion{minimum_api_version, available}});
    }
    return result::success<result_type>();
}
}  // namespace vulkandemo::vulkan::detail
