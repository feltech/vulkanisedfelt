// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./video.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <array>
#include <functional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

#include <SDL.h>
#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/monad/state_io.hpp>
#include <fnfelt/result.hpp>
#include <immer/box.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "detail/test_runtime.hpp"

namespace vulkandemo::sdl
{
namespace
{
using backend::MockApi;
using mock_environment = detail::TestState<MockApi>;
using mock_state = immer::box<mock_environment>;
}  // namespace

TEST_CASE("[impure] SDL video acquisition is lazy and main-owned")
{
    MockApi api;
    std::thread::id const main_thread = std::this_thread::get_id();
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO))
        .WITH(std::this_thread::get_id() == main_thread)
        .RETURN(0);
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO))
        .WITH(std::this_thread::get_id() == main_thread);

    detail::TestRuntime runtime{api};
    auto plan = state_io_sdl_initialize_video<mock_state>();
    [[maybe_unused]] auto discarded = plan.build(runtime.state());

    sdl_result<void> result = runtime.run(plan);

    REQUIRE(fnfelt::result::is_success(result));
    CHECK(runtime.resources().video_owner != nullptr);
    // Returned state retains video until its final resource snapshot is released.
}

TEST_CASE("[impure] SDL initialization failure owns its error and acquires nothing")
{
    MockApi api;
    std::array error = std::to_array("video fail");
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).RETURN(-1);
    REQUIRE_CALL(api, sdl_get_error()).RETURN(error.data());

    detail::TestRuntime runtime{api};
    mock_state snapshot = runtime.state();
    sdl_result<void> result = runtime.run(state_io_sdl_initialize_video<mock_state>());
    error[0] = 'X';

    REQUIRE(fnfelt::result::is_error(result));
    SdlFailure const & failure = fnfelt::result::get_error(result);
    CHECK(failure.operation == FallibleSdlOperation::initialize_video);
    CHECK(failure.message == immer::array<char>{'v', 'i', 'd', 'e', 'o', ' ', 'f', 'a', 'i', 'l'});
    CHECK_FALSE(runtime.resources().video_owner != nullptr);
    CHECK(&runtime.resources() == &snapshot.get().sdl_resources);
    CHECK(&runtime.state().get() == &snapshot.get());
}

TEST_CASE("[impure] SDL video StateIO preserves earlier immutable resource snapshots")
{
    MockApi api;
    bool is_video_stopped = false;
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).RETURN(0);
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO)).LR_SIDE_EFFECT(is_video_stopped = true);
    detail::TestRuntime runtime{api};
    mock_state initial = runtime.state();

    REQUIRE(fnfelt::result::is_success(runtime.run(state_io_sdl_initialize_video<mock_state>())));
    mock_state acquired = runtime.state();

    CHECK_FALSE(initial.get().sdl_resources.video_owner);
    CHECK(acquired.get().sdl_resources.video_owner != nullptr);
    CHECK(&initial.get() != &acquired.get());

    auto release = fnfelt::monad::state_io::just<mock_state>(0).store(
        [](int const &, mock_state state)
        {
            return std::move(state).update(
                [](mock_environment snapshot)
                {
                    snapshot.sdl_resources.video_owner.reset();
                    return snapshot;
                });
        });
    CHECK_EQ(runtime.run(release), 0);

    CHECK_FALSE(runtime.resources().video_owner);
    CHECK(acquired.get().sdl_resources.video_owner != nullptr);
    CHECK_FALSE(is_video_stopped);

    acquired = initial;
    CHECK(is_video_stopped);
}

TEST_CASE("[impure] SDL StateIO errors and stops preserve resources without storing")
{
    namespace state_io = fnfelt::monad::state_io;
    namespace io = fnfelt::monad::io;
    using outcome = sdl_result<int>;
    MockApi api;
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).RETURN(0);
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO));
    detail::TestRuntime runtime{api};
    mock_state acquired = runtime.state();
    bool is_mutator_called = false;
    auto clear = [&is_mutator_called](int const &, mock_state state)
    {
        is_mutator_called = true;
        return std::move(state).update(
            [](mock_environment snapshot)
            {
                snapshot.sdl_resources = {};
                return snapshot;
            });
    };
    outcome terminal = fnfelt::result::stopped<outcome>();
    bool is_error_outcome = false;
    SUBCASE("error after successful acquisition")
    {
        terminal = fnfelt::result::error<outcome>(
            SdlFailure{FallibleSdlOperation::initialize_video, immer::array<char>{'e'}});
        is_error_outcome = true;
    }
    SUBCASE("stop after successful acquisition") {}
    auto plan =
        state_io_sdl_initialize_video<mock_state>()
            .store(
                [&acquired](mock_state state)
                {
                    acquired = state;
                    return state;
                })
            .and_then([terminal = std::move(terminal), clear]
                      { return state_io::lift<mock_state>(io::just(terminal)).store(clear); });

    outcome result = runtime.run(plan);

    CHECK(is_error_outcome ? fnfelt::result::is_error(result) : fnfelt::result::is_stopped(result));
    CHECK(&runtime.state().get() == &acquired.get());
    CHECK(runtime.resources().video_owner != nullptr);
    CHECK_FALSE(is_mutator_called);
}

TEST_CASE("[impure] SDL resource snapshots share video cleanup until final release")
{
    MockApi api;
    bool is_video_stopped = false;
    std::thread::id const main_thread = std::this_thread::get_id();
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).RETURN(0);
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO))
        .WITH(std::this_thread::get_id() == main_thread)
        .LR_SIDE_EFFECT(is_video_stopped = true);
    detail::TestRuntime runtime{api};
    REQUIRE(fnfelt::result::is_success(runtime.run(state_io_sdl_initialize_video<mock_state>())));

    // A copied snapshot retains video after the original holder relinquishes it.
    SdlResources snapshot = runtime.resources();
    auto release = fnfelt::monad::state_io::just<mock_state>(0).store(
        [](int const &, mock_state state)
        {
            return std::move(state).update(
                [](mock_environment snapshot)
                {
                    snapshot.sdl_resources.video_owner.reset();
                    return snapshot;
                });
        });
    CHECK_EQ(runtime.run(release), 0);

    CHECK_FALSE(is_video_stopped);

    // Moving the snapshot transfers its share; final release performs cleanup once.
    SdlResources moved = std::move(snapshot);
    CHECK_FALSE(snapshot.video_owner);
    moved.video_owner.reset();

    CHECK(is_video_stopped);
}

TEST_CASE("[impure] SDL video released by a pool worker can be acquired again")
{
    namespace state_io = fnfelt::monad::state_io;
    MockApi api;
    trompeloeil::sequence order;
    std::thread::id const main_thread = std::this_thread::get_id();
    int cleanup_calls = 0;
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).IN_SEQUENCE(order).RETURN(0);
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO))
        .WITH(std::this_thread::get_id() == main_thread)
        .IN_SEQUENCE(order)
        .LR_SIDE_EFFECT(++cleanup_calls);
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).IN_SEQUENCE(order).RETURN(0);
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO))
        .WITH(std::this_thread::get_id() == main_thread)
        .IN_SEQUENCE(order);
    detail::TestRuntime runtime{api};
    REQUIRE(fnfelt::result::is_success(runtime.run(state_io_sdl_initialize_video<mock_state>())));

    // Release the final owner during worker execution, while main services cleanup.
    auto release = state_io::just<mock_state>(0).store(
        [main_thread](int const &, mock_state state)
        {
            CHECK(std::this_thread::get_id() != main_thread);
            return std::move(state).update(
                [](mock_environment snapshot)
                {
                    snapshot.sdl_resources.video_owner.reset();
                    return snapshot;
                });
        });
    CHECK_EQ(runtime.run(release), 0);

    CHECK_EQ(cleanup_calls, 1);
    CHECK_FALSE(runtime.resources().video_owner);
    CHECK(fnfelt::result::is_success(runtime.run(state_io_sdl_initialize_video<mock_state>())));
}

TEST_CASE("[impure] SDL runtime completes for returned error stopped and thrown outcomes")
{
    namespace reader_io = fnfelt::monad::reader_io;
    namespace io = fnfelt::monad::io;
    using outcome = sdl_result<int>;
    MockApi api;
    detail::TestRuntime runtime{api};

    auto failure = reader_io::lift<mock_environment>(io::just(
        fnfelt::result::error<outcome>(
            SdlFailure{FallibleSdlOperation::initialize_video, immer::array<char>{'e'}})));
    auto stopped = reader_io::lift<mock_environment>(io::just(fnfelt::result::stopped<outcome>()));
    auto throwing = reader_io::lift<mock_environment>(
        io::create([]() -> int { throw std::runtime_error("root"); }));

    CHECK(fnfelt::result::is_error(runtime.run(failure)));
    CHECK(fnfelt::result::is_stopped(runtime.run(stopped)));
    CHECK_THROWS_AS(runtime.run(throwing), std::runtime_error);
}

TEST_CASE("[integration][windowed] SDL video initializes and releases on main")
{
    backend::Api api;
    using state_type = immer::box<detail::TestState<backend::Api>>;
    detail::TestRuntime runtime{api};

    sdl_result<void> result = runtime.run(state_io_sdl_initialize_video<state_type>());

    REQUIRE(fnfelt::result::is_success(result));
    CHECK(runtime.resources().video_owner != nullptr);
}
}  // namespace vulkandemo::sdl

namespace vulkandemo::backend
{
namespace
{
template <class TApi>
struct SdlResourceEnvironment
{
    std::reference_wrapper<TApi> api;
    lf::worker_context * main_context;
    sdl::SdlResources sdl_resources;
};

struct GetOnlyResources
{
    sdl::SdlResources & get() const;
};

struct GetOnlyResourceEnvironment
{
    std::reference_wrapper<Api> api;
    lf::worker_context * main_context;
    GetOnlyResources sdl_resources;
};

static_assert(std::is_aggregate_v<sdl::SdlResources>);
static_assert(std::is_copy_constructible_v<sdl::SdlResources>);
static_assert(std::is_move_constructible_v<sdl::SdlResources>);
using video_state = immer::box<SdlResourceEnvironment<Api>>;
static_assert(sdl::detail::sdl_initialize_video_state<video_state>);
static_assert(!sdl::detail::sdl_initialize_video_state<video_state &>);
static_assert(!sdl::detail::sdl_initialize_video_state<video_state const &>);
static_assert(!sdl::detail::sdl_initialize_video_state<SdlResourceEnvironment<Api>>);
static_assert(!sdl::detail::sdl_initialize_video_state<GetOnlyResourceEnvironment>);
static_assert(!sdl::detail::sdl_initialize_video_state<immer::box<GetOnlyResourceEnvironment>>);
}  // namespace
}  // namespace vulkandemo::backend
#endif  // DOCTEST_CONFIG_DISABLE
