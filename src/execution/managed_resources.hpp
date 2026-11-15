// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file execution/managed_resources.hpp
 *
 * Shared resource ownership with synchronous main-thread cleanup.
 */
#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include <libfork/core/ext/context.hpp>

#include "./managed_resources_types.hpp"
#include "detail/managed_resources.hpp"

namespace vulkandemo::execution
{
/**
 * Shares a resource whose final release waits for main-thread deletion.
 *
 * The context must remain alive and serviced through final release. Allocation,
 * thread creation and cleanup failures are terminal.
 *
 * @tparam T Resource type.
 * @tparam TDeleter Cleanup callable type.
 * @param resource Acquired resource.
 * @param context Serviced main-thread context.
 * @param deleter Action destroying the resource.
 * @return Shared resource owner.
 */
template <class T, class TDeleter>
auto make_main_thread_ptr(T * resource, lf::worker_context * context, TDeleter deleter) noexcept
    -> std::shared_ptr<T> pre(context != nullptr)
{
    return {resource, detail::MainThreadDeleter<TDeleter>{context, std::move(deleter)}};
}

/**
 * Shares pointerless cleanup whose final release waits for the main thread.
 *
 * The context must remain alive and serviced through final release. Allocation,
 * thread creation and cleanup failures are terminal.
 *
 * @tparam TCleanup Cleanup callable type.
 * @param context Serviced main-thread context.
 * @param cleanup Action performed on final release.
 * @return Shared deferred cleanup.
 */
template <class TCleanup>
auto make_main_thread_deferred(lf::worker_context * context, TCleanup && cleanup) noexcept
    -> std::shared_ptr<resource_guard> pre(context != nullptr)
{
    return std::make_shared<resource_guard>(
        resource_cleanup{detail::MainThreadCleanup<std::decay_t<TCleanup>>{
            context, std::forward<TCleanup>(cleanup)}});
}
}  // namespace vulkandemo::execution
