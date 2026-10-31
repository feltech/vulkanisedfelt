// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./mock_api.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <string_view>

#include <meta>

#include "./api.hpp"

namespace vulkandemo::backend
{
namespace
{
consteval std::string_view validate_api(
    std::meta::info implementation, std::meta::info contract = ^^Api)
{
    auto const access = std::meta::access_context::unprivileged();
    for (auto command : members_of(contract, access))
    {
        if (is_function_template(command) && is_public(command))
        {
            return "API commands must have concrete signatures";
        }
        if (!is_function(command) || is_special_member_function(command) || !is_public(command) ||
            is_static_member(command))
        {
            continue;
        }
        bool has_match = false;
        for (auto candidate : members_of(implementation, access))
        {
            if (is_function(candidate) && has_identifier(candidate) && is_public(candidate) &&
                !is_static_member(candidate) &&
                identifier_of(candidate) == identifier_of(command) &&
                is_same_type(type_of(candidate), type_of(command)))
            {
                has_match = true;
            }
        }
        if (!has_match)
        {
            return "missing public Vulkan command with matching signature";
        }
    }
    return {};
}
static_assert(validate_api(^^MockApi).empty(), "MockApi must match Api commands");

}  // namespace
}  // namespace vulkandemo::backend
#endif  // DOCTEST_CONFIG_DISABLE
