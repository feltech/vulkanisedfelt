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

#include <libfork/core/ext/context.hpp>
#include <libfork/core/ext/resume.hpp>
#include <libfork/core/ext/tls.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include "../resources.hpp"

namespace vulkandemo::sdl::detail
{
/** Borrowed capabilities for one platform test. */
template <class TApi>
struct TestEnvironment
{
    /** Borrowed command implementation. */
    std::reference_wrapper<TApi> api;
    /** Borrowed main-owned anchors. */
    std::reference_wrapper<SdlResources> sdl_resources;
    /** Serviced main-thread context. */
    lf::worker_context * main_context;
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
        : api_{api},
          main_context_{
              lf::worker_init(lf::nullary_function_t{[this] noexcept { notify(); }}), lf::finalize}
    {
    }

    /**
     * Supplies borrowed test capabilities.
     *
     * @return Execution environment for this run.
     */
    auto environment() -> TestEnvironment<TApi>
    {
        return {std::ref(api_), std::ref(resources_), main_context_.get()};
    }

    /**
     * Observes anchors after a run completes.
     *
     * @return Main-owned test resources.
     */
    auto resources() -> SdlResources const &
    {
        return resources_;
    }

    /**
     * Executes a worker program while main services its submissions.
     *
     * @tparam TPlan ReaderIO plan type.
     * @param plan Deferred program.
     * @return Original program outcome after joining its worker.
     */
    template <class TPlan>
    auto run(TPlan const & plan)
    {
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
                        return plan.build(environment()).sync_wait(pool);
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
        return worker.get();
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

    TApi & api_;
    std::atomic<bool> should_main_thread_awake_{false};
    std::atomic<bool> is_program_finished_{false};
    std::unique_ptr<lf::worker_context, decltype(&lf::finalize)> main_context_;
    // Destroy main-owned SDL resources before finalizing the dispatcher.
    SdlResources resources_;
};
}  // namespace vulkandemo::sdl::detail
#endif  // DOCTEST_CONFIG_DISABLE
