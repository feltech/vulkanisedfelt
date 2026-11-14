// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./main_thread.hpp"

#include <thread>

#ifndef DOCTEST_CONFIG_DISABLE
#include <future>

#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::sdl
{
namespace
{
// This executable relies on GCC/Linux initializing this source-file global before main.
// ISO C++ permits deferred initialization, so this is a platform assumption.
std::thread::id const main_thread_id = std::this_thread::get_id();
}  // namespace

auto is_main_thread() noexcept -> bool
{
    return std::this_thread::get_id() == main_thread_id;
}

#ifndef DOCTEST_CONFIG_DISABLE
TEST_CASE("[integration][headless] main-thread identity distinguishes an asynchronous worker")
{
    auto worker = std::async(std::launch::async, [] { return is_main_thread(); });

    CHECK_FALSE(worker.get());
    CHECK(is_main_thread());
}
#endif  // DOCTEST_CONFIG_DISABLE
}  // namespace vulkandemo::sdl
