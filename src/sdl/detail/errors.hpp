// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/errors.hpp
 *
 * Immediate ownership of SDL error text.
 */
#pragma once

#include <cstring>
#include <functional>

#include "../errors.hpp"

namespace vulkandemo::sdl::detail
{
/**
 * Copies the last SDL error before another operation can replace it.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed SDL implementation.
 * @param operation Operation that failed.
 * @return Owned error details.
 */
template <class TApi>
auto last_failure(std::reference_wrapper<TApi> api, FallibleSdlOperation operation) -> SdlFailure
{
    auto const * message = api.get().sdl_get_error();
    return {operation, immer::array<char>{message, message + std::strlen(message)}};
}
}  // namespace vulkandemo::sdl::detail
