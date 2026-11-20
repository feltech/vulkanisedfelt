// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/test_runtime.hpp
 *
 * Test-only main-thread dispatch for mocked and real SDL actions.
 */
#pragma once

#ifndef DOCTEST_CONFIG_DISABLE
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <utility>

#include <meta>

#include <fnfelt/monad/state_io.hpp>
#include <immer/box.hpp>
#include <libfork/core/ext/context.hpp>
#include <libfork/core/ext/resume.hpp>
#include <libfork/core/ext/tls.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../resources.hpp"

namespace vulkandemo::sdl::detail
{
/** Owned resource state and borrowed capabilities for one platform test. */
template <class TApi>
struct TestState
{
    /** Borrowed command implementation. */
    std::reference_wrapper<TApi> api;
    /** Serviced main-thread context. */
    lf::worker_context * main_context;
    /** Owned SDL resources. */
    SdlResources sdl_resources;
};

/** Keeps main-thread anchors alive until test borrowers and cleanup finish. */
template <class TApi>
class TestRuntime
{
public:
    /**
     * Initializes the main context before creating resource anchors.
     *
     * @param api Borrowed command implementation alive through cleanup.
     */
    explicit TestRuntime(TApi & api)
        : main_context_{lf::worker_init(lf::nullary_function_t{[this] noexcept { notify(); }}), lf::finalize},
          state_{TestState<TApi>{std::ref(api), main_context_.get(), {}}}
    {
    }

    /**
     * Provides a snapshot of execution state and borrowed capabilities.
     *
     * @return Execution environment for this run.
     */
    auto environment() const -> TestState<TApi>
    {
        return state_.get();
    }

    /**
     * Provides the immutable execution-state snapshot.
     *
     * @return Boxed state for this run.
     */
    auto state() const -> immer::box<TestState<TApi>>
    {
        return state_;
    }

    /**
     * Observes anchors after a run completes.
     *
     * @return Main-owned test resources.
     */
    auto resources() const -> SdlResources const &
    {
        return state_.get().sdl_resources;
    }

    /**
     * Executes a worker program while main services its submissions.
     *
     * @tparam TPlan ReaderIO or StateIO plan type.
     * @param plan Deferred program.
     * @return Original program outcome after joining its worker.
     */
    template <class TPlan>
    auto run(TPlan const & plan)
    {
        constexpr bool is_state_io = fnfelt::monad::state_io::detail::is_state_io(^^TPlan);
        is_program_finished_.store(false, std::memory_order_relaxed);
        auto worker = std::async(
            std::launch::async,
            [&]
            {
                try
                {
                    // Pool teardown precedes completion, including error/stopped Results.
                    auto result = [&]
                    {
                        lf::lazy_pool pool{2};
                        if constexpr (is_state_io)
                        {
                            // Transfer the only snapshot so worker releases can be final releases.
                            return plan.build(std::move(state_)).sync_wait(pool);
                        }
                        else
                        {
                            return plan.build(environment()).sync_wait(pool);
                        }
                    }();
                    complete();
                    return result;
                }
                catch (...)
                {
                    complete();
                    throw;
                }
            });

        for (;;)
        {
            // Consume notifications before checking the queue.
            should_main_thread_awake_.exchange(false, std::memory_order_acquire);
            if (auto * jobs = main_context_->try_pop_all())
            {
                lf::resume(jobs);
            }
            if (is_program_finished_.load(std::memory_order_relaxed))
            {
                break;
            }
            should_main_thread_awake_.wait(false, std::memory_order_relaxed);
        }
        auto result = worker.get();
        if constexpr (is_state_io)
        {
            state_ = std::move(result.second);
            return std::move(result.first);
        }
        else
        {
            return result;
        }
    }

private:
    void notify() noexcept
    {
        should_main_thread_awake_.exchange(true, std::memory_order_release);
        should_main_thread_awake_.notify_one();
    }

    void complete() noexcept
    {
        is_program_finished_.store(true, std::memory_order_relaxed);
        notify();
    }

    std::atomic<bool> should_main_thread_awake_{false};
    std::atomic<bool> is_program_finished_{false};
    std::unique_ptr<lf::worker_context, decltype(&lf::finalize)> main_context_;
    // Destroy the final resource snapshot before finalizing the dispatcher.
    immer::box<TestState<TApi>> state_;
};
}  // namespace vulkandemo::sdl::detail
#endif  // DOCTEST_CONFIG_DISABLE
