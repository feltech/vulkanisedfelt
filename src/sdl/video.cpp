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
#include <fnfelt/result.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "detail/test_runtime.hpp"

namespace vulkandemo::sdl
{
namespace
{
using backend::MockApi;
using mock_environment = detail::TestEnvironment<MockApi>;
}  // namespace

TEST_CASE("[impure] SDL video acquisition is lazy and main-owned")
{
    MockApi api;
    auto const main_thread = std::this_thread::get_id();
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO))
        .WITH(std::this_thread::get_id() == main_thread)
        .RETURN(0);
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO))
        .WITH(std::this_thread::get_id() == main_thread);

    detail::TestRuntime runtime{api};
    auto plan = reader_io_sdl_initialize_video<mock_environment>();
    [[maybe_unused]] auto discarded = plan.build(runtime.environment());

    auto result = runtime.run(plan);

    REQUIRE(fnfelt::result::is_success(result));
    CHECK(runtime.resources().video_owner != nullptr);
    // The main-thread holder performs cleanup after worker execution finishes.
}

TEST_CASE("[impure] SDL initialization failure owns its error and acquires nothing")
{
    MockApi api;
    auto error = std::to_array("video fail");
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).RETURN(-1);
    REQUIRE_CALL(api, sdl_get_error()).RETURN(error.data());

    detail::TestRuntime runtime{api};
    auto result = runtime.run(reader_io_sdl_initialize_video<mock_environment>());
    error[0] = 'X';

    REQUIRE(fnfelt::result::is_error(result));
    auto const & failure = fnfelt::result::get_error(result);
    CHECK(failure.operation == FallibleSdlOperation::initialize_video);
    CHECK(failure.message == immer::array<char>{'v', 'i', 'd', 'e', 'o', ' ', 'f', 'a', 'i', 'l'});
    CHECK_FALSE(runtime.resources().video_owner != nullptr);
}

TEST_CASE("[impure] SDL resource snapshots share video cleanup until final release")
{
    MockApi api;
    bool is_video_stopped = false;
    auto const main_thread = std::this_thread::get_id();
    REQUIRE_CALL(api, sdl_init_sub_system(SDL_INIT_VIDEO)).RETURN(0);
    REQUIRE_CALL(api, sdl_quit_sub_system(SDL_INIT_VIDEO))
        .WITH(std::this_thread::get_id() == main_thread)
        .LR_SIDE_EFFECT(is_video_stopped = true);
    detail::TestRuntime runtime{api};
    REQUIRE(
        fnfelt::result::is_success(
            runtime.run(reader_io_sdl_initialize_video<mock_environment>())));

    // A copied snapshot retains video after the original holder relinquishes it.
    auto snapshot = runtime.resources();
    runtime.environment().sdl_resources.get().video_owner.reset();

    CHECK_FALSE(is_video_stopped);

    // Moving the snapshot transfers its share; final release performs cleanup once.
    auto moved = std::move(snapshot);
    CHECK_FALSE(snapshot.video_owner);
    moved.video_owner.reset();

    CHECK(is_video_stopped);
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
    using environment_type = detail::TestEnvironment<backend::Api>;
    detail::TestRuntime runtime{api};

    auto result = runtime.run(reader_io_sdl_initialize_video<environment_type>());

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
    std::reference_wrapper<sdl::SdlResources> sdl_resources;
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
static_assert(sdl::sdl_initialize_video_environment<SdlResourceEnvironment<Api>>);
static_assert(!sdl::sdl_initialize_video_environment<GetOnlyResourceEnvironment>);
}  // namespace
}  // namespace vulkandemo::backend
#endif  // DOCTEST_CONFIG_DISABLE
