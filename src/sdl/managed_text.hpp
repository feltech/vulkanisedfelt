// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/managed_text.hpp
 *
 * Deferred release of shared SDL text allocations.
 */
#pragma once

#include <functional>
#include <utility>

#include "./managed_text_types.hpp"

namespace vulkandemo::sdl
{
/**
 * Shares immutable SDL text until its final owner releases it.
 *
 * The borrowed API must outlive every owner. Its allocator must support release
 * on the thread destroying the final owner.
 *
 * @tparam TApi SDL allocation implementation.
 * @param buffer SDL-allocated terminated bytes, or null.
 * @param api Borrowed matching SDL implementation.
 * @return Shared ownership, or an empty owner for null.
 */
template <class TApi>
[[nodiscard]] auto make_text_ptr(char * buffer, TApi & api) -> text_ptr
{
    if (!buffer)
    {
        return {};
    }

    // Deleter releasing the bytes through their matching SDL implementation.
    auto free_text = std::bind_front(&TApi::sdl_free, std::ref(api));
    return text_ptr{buffer, std::move(free_text)};
}
}  // namespace vulkandemo::sdl
