// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./managed_resources.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <utility>

#include <doctest/doctest.h>
#include <fnfelt/monad/io.hpp>
#include <libfork/core/ext/resume.hpp>
#include <libfork/core/ext/tls.hpp>
#include <libfork/schedule/lazy_pool.hpp>

namespace vulkandemo::execution
{
namespace
{
template <class TFactory>
auto run_with_main(TFactory factory)
{
    std::atomic<bool> should_main_thread_awake{false};
    std::atomic<bool> is_program_finished{false};
    auto notify = [&] noexcept
    {
        should_main_thread_awake.exchange(true, std::memory_order_release);
        should_main_thread_awake.notify_one();
    };
    std::unique_ptr<lf::worker_context, decltype(&lf::finalize)> main_context{
        lf::worker_init(lf::nullary_function_t{notify}), lf::finalize};
    auto action = std::invoke(factory, main_context.get());
    auto worker = std::async(
        std::launch::async,
        [&]
        {
            auto result = [&]
            {
                lf::lazy_pool pool{1};
                return action.build().sync_wait(pool);
            }();
            is_program_finished.store(true, std::memory_order_relaxed);
            notify();
            return result;
        });
    for (;;)
    {
        should_main_thread_awake.exchange(false, std::memory_order_acquire);
        if (auto * jobs = main_context->try_pop_all())
        {
            lf::resume(jobs);
        }
        if (is_program_finished.load(std::memory_order_relaxed))
        {
            break;
        }
        should_main_thread_awake.wait(false, std::memory_order_relaxed);
    }
    return worker.get();
}
}  // namespace

TEST_CASE("[impure] main-thread resource owners clean up inline exactly once")
{
    namespace io = fnfelt::monad::io;
    int pointer_calls = 0;
    int deferred_calls = 0;

    run_with_main(
        [&](lf::worker_context * context)
        {
            std::shared_ptr pointer = make_main_thread_ptr(
                new int{42},
                context,
                [&](int * owned) noexcept
                {
                    CHECK(is_main_thread());
                    ++pointer_calls;
                    delete owned;
                });
            std::shared_ptr deferred = make_main_thread_deferred(
                context,
                [&] noexcept
                {
                    CHECK(is_main_thread());
                    ++deferred_calls;
                });

            // Copies retain ownership; the final main-thread release completes inline.
            std::shared_ptr pointer_copy{pointer};
            std::shared_ptr deferred_copy{deferred};
            pointer.reset();
            deferred.reset();
            CHECK_EQ(pointer_calls, 0);
            CHECK_EQ(deferred_calls, 0);
            pointer_copy.reset();
            deferred_copy.reset();

            CHECK_EQ(pointer_calls, 1);
            CHECK_EQ(deferred_calls, 1);
            return io::just(0);
        });
}

TEST_CASE("[impure] final pointer release on a pool worker destroys dependencies on main")
{
    namespace io = fnfelt::monad::io;
    int stage = 0;
    std::weak_ptr<int> observer;

    run_with_main(
        [&](lf::worker_context * context)
        {
            std::shared_ptr parent = make_main_thread_deferred(
                context,
                [&] noexcept
                {
                    CHECK(is_main_thread());
                    CHECK_EQ(stage, 1);
                    stage = 2;
                });
            std::shared_ptr pointer = make_main_thread_ptr(
                new int{42},
                context,
                [&, parent = std::move(parent)](int * owned) mutable noexcept
                {
                    CHECK(is_main_thread());
                    CHECK_EQ(stage, 0);
                    stage = 1;
                    delete owned;
                    parent.reset();
                });
            observer = pointer;
            std::shared_ptr pending{std::make_shared<std::shared_ptr<int>>(std::move(pointer))};

            // The extra holder lets copied IO plans share the same final resource owner.
            return io::create(
                [pending]
                {
                    CHECK_FALSE(is_main_thread());
                    pending->reset();
                    return 0;
                });
        });

    CHECK(observer.expired());
    CHECK_EQ(stage, 2);
}

TEST_CASE("[impure] final pointerless release on a pool worker runs move-only cleanup on main")
{
    namespace io = fnfelt::monad::io;
    int cleanup_calls = 0;

    run_with_main(
        [&](lf::worker_context * context)
        {
            std::shared_ptr deferred = make_main_thread_deferred(
                context,
                [&, token = std::make_unique<int>(42)] noexcept
                {
                    CHECK(is_main_thread());
                    CHECK_EQ(*token, 42);
                    ++cleanup_calls;
                });
            std::shared_ptr pending{std::make_shared<decltype(deferred)>(std::move(deferred))};

            return io::create(
                [pending]
                {
                    CHECK_FALSE(is_main_thread());
                    pending->reset();
                    return 0;
                });
        });

    CHECK_EQ(cleanup_calls, 1);
}

TEST_CASE("[impure] weak observers do not retain dependencies copied by deleter moves")
{
    namespace io = fnfelt::monad::io;
    struct CopyingParentDeleter
    {
        std::shared_ptr<int> const parent;
        int & cleanup_calls;

        void operator()(int * owned) const noexcept
        {
            CHECK(is_main_thread());
            ++cleanup_calls;
            delete owned;
        }
    };
    int cleanup_calls = 0;
    std::weak_ptr<int> parent_observer;
    std::weak_ptr<int> resource_observer;

    run_with_main(
        [&](lf::worker_context * context)
        {
            std::shared_ptr parent{std::make_shared<int>(7)};
            parent_observer = parent;
            std::shared_ptr pointer = make_main_thread_ptr(
                new int{42}, context, CopyingParentDeleter{std::move(parent), cleanup_calls});
            resource_observer = pointer;
            std::shared_ptr pending{std::make_shared<std::shared_ptr<int>>(std::move(pointer))};

            // A const parent member is copied even when its containing deleter is moved.
            CHECK_FALSE(parent_observer.expired());
            return io::create(
                [pending]
                {
                    CHECK_FALSE(is_main_thread());
                    pending->reset();
                    return 0;
                });
        });

    // The main task has finished destroying its captures; weak resource ownership remains.
    CHECK(resource_observer.expired());
    CHECK(parent_observer.expired());
    CHECK_EQ(cleanup_calls, 1);
}
}  // namespace vulkandemo::execution
#endif  // DOCTEST_CONFIG_DISABLE
