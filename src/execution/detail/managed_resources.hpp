// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file execution/detail/managed_resources.hpp
 *
 * Main-thread dispatch for shared resource cleanup.
 */
#pragma once

#include <functional>
#include <optional>
#include <thread>
#include <utility>

#include <libfork/core/ext/context.hpp>
#include <libfork/core/sync_wait.hpp>
#include <libfork/core/task.hpp>

#include "../main_thread.hpp"

namespace vulkandemo::execution::detail
{
/**
 * Submits cleanup from a thread without libfork worker state.
 *
 * @tparam TCleanup Cleanup callable type.
 * @param context Serviced main-thread context.
 * @param cleanup Owned cleanup action.
 */
template <class TCleanup>
void cleanup_from_external_thread(lf::worker_context * context, TCleanup cleanup) noexcept
{
    lf::sync_wait(
        *context,
        [](auto, TCleanup action) noexcept -> lf::task<>
        {
            contract_assert(is_main_thread());
            std::invoke(action);
            co_return;
        },
        std::move(cleanup));
}

/**
 * Completes cleanup on the main thread before returning.
 *
 * @tparam TCleanup Cleanup callable type.
 * @param context Serviced main-thread context.
 * @param cleanup Owned cleanup action.
 */
template <class TCleanup>
void cleanup_on_main(lf::worker_context * context, TCleanup cleanup) noexcept
{
    if (is_main_thread())
    {
        std::invoke(cleanup);
    }
    else
    {
        // A fresh thread can submit a root even when the releasing thread is a libfork worker.
        // Joining keeps cleanup complete before the final owner's destructor returns.
        std::jthread worker{cleanup_from_external_thread<TCleanup>, context, std::move(cleanup)};
    }
}

/**
 * Dispatches a shared owner's final deletion to main.
 *
 * @tparam TDeleter Resource deleter type.
 */
template <class TDeleter>
struct MainThreadDeleter
{
    /** Serviced main-thread context. */
    lf::worker_context * context;
    /** Cleanup retained until the last owner releases the resource. */
    std::optional<TDeleter> deleter;

    /**
     * Destroys the resource on main before returning.
     *
     * @param resource Resource released by its final owner.
     */
    void operator()(auto * resource) noexcept
    {
        // Moving may copy const captures, so destroy the moved-from deleter immediately.
        // Weak observers can keep its control block alive after the resource is gone.
        auto cleanup = std::bind_front(std::move(*deleter), resource);
        deleter.reset();
        cleanup_on_main(context, std::move(cleanup));
    }
};

/**
 * Dispatches a shared guard's final cleanup to main.
 *
 * @tparam TCleanup Cleanup callable type.
 */
template <class TCleanup>
struct MainThreadCleanup
{
    /** Serviced main-thread context. */
    lf::worker_context * context;
    /** Cleanup retained until the guard's final release. */
    TCleanup cleanup;

    /** Completes cleanup on main before returning. */
    void operator()() noexcept
    {
        cleanup_on_main(context, std::move(cleanup));
    }
};
}  // namespace vulkandemo::execution::detail
