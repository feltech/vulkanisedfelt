// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./managed_text.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <thread>
#include <utility>

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>

#include "../backend/mock_api.hpp"
#include "./managed_text_types.hpp"

namespace vulkandemo::sdl
{
TEST_CASE("[impure] shared SDL text releases its original allocation with the last owner")
{
    backend::MockApi api;
    char buffer[] = "owned by SDL";
    bool is_freed = false;
    REQUIRE_CALL(api, sdl_free(static_cast<void *>(buffer))).LR_SIDE_EFFECT(is_freed = true);

    {
        text_ptr owner = make_text_ptr(buffer, api);
        CHECK_EQ(owner.get(), buffer);
        text_ptr shared = owner;
        owner.reset();

        // A copied event owner keeps the original bytes alive without another allocation.
        CHECK_FALSE(is_freed);
        CHECK_EQ(shared.get(), buffer);
    }

    CHECK(is_freed);
}

TEST_CASE("[impure] null SDL text has no allocation to release")
{
    backend::MockApi api;

    text_ptr owner = make_text_ptr(nullptr, api);

    CHECK_FALSE(owner);
}

TEST_CASE("[impure] SDL text is released on the final owner's thread")
{
    backend::MockApi api;
    char buffer[] = "transferred";
    std::thread::id const main_thread = std::this_thread::get_id();
    bool is_freed = false;
    REQUIRE_CALL(api, sdl_free(static_cast<void *>(buffer)))
        .WITH(std::this_thread::get_id() != main_thread)
        .LR_SIDE_EFFECT(is_freed = true);
    text_ptr owner = make_text_ptr(buffer, api);

    std::thread worker{[](text_ptr transferred) { transferred.reset(); }, std::move(owner)};
    worker.join();

    CHECK_FALSE(owner);
    CHECK(is_freed);
}
}  // namespace vulkandemo::sdl
#endif  // DOCTEST_CONFIG_DISABLE
