// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./instance_extensions.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <array>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>

#include <SDL.h>
#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>

#include "../backend/api.hpp"
#include "../backend/mock_api.hpp"
#include "./video.hpp"
#include "./window.hpp"
#include "./window_types.hpp"
#include "detail/test_runtime.hpp"

namespace vulkandemo::sdl
{
namespace
{
using backend::MockApi;
using mock_environment = detail::TestEnvironment<MockApi>;

auto extension_name(char const * name) -> text::String
{
    return text::String{name};
}
}  // namespace

TEST_CASE("[impure] SDL required extension discovery is deferred")
{
    MockApi api;
    detail::TestRuntime runtime{api};
    char window_storage{};
    auto * window = reinterpret_cast<SDL_Window *>(&window_storage);

    auto plan = reader_io_sdl_vulkan_get_instance_extensions<mock_environment>(window);
    [[maybe_unused]] auto discarded = plan.build(runtime.environment());

    // No expectations: preparing or discarding the query performs no API effects.
}

TEST_CASE("[impure] SDL required extensions can be empty without a fill call")
{
    MockApi api;
    auto const main_thread = std::this_thread::get_id();
    char window_storage{};
    auto * window = reinterpret_cast<SDL_Window *>(&window_storage);
    REQUIRE_CALL(api, sdl_vulkan_get_instance_extensions(window, trompeloeil::_, nullptr))
        .WITH(std::this_thread::get_id() == main_thread)
        .SIDE_EFFECT(*_2 = 0)
        .RETURN(SDL_TRUE);
    detail::TestRuntime runtime{api};

    auto result =
        runtime.run(reader_io_sdl_vulkan_get_instance_extensions<mock_environment>(window));

    REQUIRE(fnfelt::result::is_success(result));
    CHECK(fnfelt::result::get_success(result).empty());
}

TEST_CASE("[impure] SDL required extensions own exact or shortened snapshots across replay")
{
    MockApi api;
    auto const main_thread = std::this_thread::get_id();
    char window_storage{};
    auto * window = reinterpret_cast<SDL_Window *>(&window_storage);
    std::array first{'V', 'K', '_', 'f', 'i', 'r', 's', 't', '\0'};
    std::array second{'V', 'K', '_', 's', 'e', 'c', 'o', 'n', 'd', '\0'};
    unsigned int written = 0;
    SUBCASE("empty fill")
    {
        written = 0;
    }
    SUBCASE("short fill")
    {
        written = 1;
    }
    SUBCASE("exact fill")
    {
        written = 2;
    }
    REQUIRE_CALL(api, sdl_vulkan_get_instance_extensions(window, trompeloeil::_, nullptr))
        .WITH(std::this_thread::get_id() == main_thread)
        .SIDE_EFFECT(*_2 = 2)
        .RETURN(SDL_TRUE)
        .TIMES(2);
    REQUIRE_CALL(api, sdl_vulkan_get_instance_extensions(window, trompeloeil::_, trompeloeil::_))
        .WITH(_3 != nullptr && *_2 == 2 && std::this_thread::get_id() == main_thread)
        .LR_SIDE_EFFECT(_3[0] = first.data())
        .LR_SIDE_EFFECT(_3[1] = second.data())
        .LR_SIDE_EFFECT(*_2 = written)
        .RETURN(SDL_TRUE)
        .TIMES(2);
    detail::TestRuntime runtime{api};
    auto plan = reader_io_sdl_vulkan_get_instance_extensions<mock_environment>(window);

    auto result = runtime.run(plan);
    auto replay = runtime.run(plan);
    first[0] = 'X';
    second[0] = 'X';

    REQUIRE(fnfelt::result::is_success(result));
    REQUIRE(fnfelt::result::is_success(replay));
    auto const & names = fnfelt::result::get_success(result);
    REQUIRE(names.size() == written);
    CHECK(fnfelt::result::get_success(replay) == names);
    if (written > 0)
        CHECK(names[0] == extension_name("VK_first"));
    if (written > 1)
        CHECK(names[1] == extension_name("VK_second"));
}

TEST_CASE("[impure] SDL required extension count and fill failures own their immediate error")
{
    MockApi api;
    auto const main_thread = std::this_thread::get_id();
    char window_storage{};
    auto * window = reinterpret_cast<SDL_Window *>(&window_storage);
    std::array message{'q', 'u', 'e', 'r', 'y', ' ', 'f', 'a', 'i', 'l', '\0'};
    bool is_fill_error = false;
    SUBCASE("count error")
    {
        is_fill_error = false;
    }
    SUBCASE("fill error")
    {
        is_fill_error = true;
    }
    trompeloeil::sequence calls;
    REQUIRE_CALL(api, sdl_vulkan_get_instance_extensions(window, trompeloeil::_, nullptr))
        .WITH(std::this_thread::get_id() == main_thread)
        .SIDE_EFFECT(*_2 = 1)
        .LR_RETURN(is_fill_error ? SDL_TRUE : SDL_FALSE)
        .IN_SEQUENCE(calls);
    std::unique_ptr<trompeloeil::expectation> fill;
    if (is_fill_error)
    {
        fill = NAMED_REQUIRE_CALL(
                   api, sdl_vulkan_get_instance_extensions(window, trompeloeil::_, trompeloeil::_))
                   .WITH(_3 != nullptr && *_2 == 1 && std::this_thread::get_id() == main_thread)
                   .RETURN(SDL_FALSE)
                   .IN_SEQUENCE(calls);
    }
    REQUIRE_CALL(api, sdl_get_error())
        .WITH(std::this_thread::get_id() == main_thread)
        .RETURN(message.data())
        .IN_SEQUENCE(calls);
    detail::TestRuntime runtime{api};

    auto result =
        runtime.run(reader_io_sdl_vulkan_get_instance_extensions<mock_environment>(window));
    message[0] = 'X';

    REQUIRE(fnfelt::result::is_error(result));
    auto const & failure = fnfelt::result::get_error(result);
    CHECK(failure.operation == FallibleSdlOperation::enumerate_instance_extensions);
    CHECK_EQ(std::string_view{failure.message.data(), failure.message.size()}, "query fail");
}

TEST_CASE("[integration][windowed] SDL required extension names outlive the window")
{
    backend::Api api;
    using environment_type = detail::TestEnvironment<backend::Api>;
    immer::array<text::String> names;

    {
        // Main-owned anchors retain video and the Vulkan-capable window during discovery.
        detail::TestRuntime runtime{api};
        auto video = runtime.run(reader_io_sdl_initialize_video<environment_type>());
        REQUIRE(fnfelt::result::is_success(video));
        WindowOptions options{
            .width = window_width{64},
            .height = window_height{48},
            .flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN)};
        auto window = runtime.run(reader_io_sdl_create_window<environment_type>(options));
        REQUIRE(fnfelt::result::is_success(window));

        auto result = runtime.run(
            reader_io_sdl_vulkan_get_instance_extensions<environment_type>(
                fnfelt::result::get_success(window)));

        REQUIRE(fnfelt::result::is_success(result));
        names = fnfelt::result::get_success(result);
    }

    // Both SDL resources are gone; the extension bytes remain independently owned.
    REQUIRE_FALSE(names.empty());
    for (auto const & name : names)
    {
        CHECK_FALSE(name.empty());
    }
}

}  // namespace vulkandemo::sdl

namespace vulkandemo::backend
{
namespace
{
template <class TApi>
struct ApiEnvironment
{
    std::reference_wrapper<TApi> api;
};

struct SdlExtensionApi
{
    SDL_bool sdl_vulkan_get_instance_extensions(SDL_Window *, unsigned int *, char const **);
    char const * sdl_get_error();
};

template <class TApi>
struct SdlQueryEnvironment
{
    std::reference_wrapper<TApi> api;
    lf::worker_context * main_context;
};

static_assert(
    sdl::sdl_vulkan_get_instance_extensions_environment<SdlQueryEnvironment<SdlExtensionApi>>);
static_assert(!sdl::sdl_initialize_video_environment<SdlQueryEnvironment<SdlExtensionApi>>);
static_assert(
    !sdl::sdl_vulkan_get_instance_extensions_environment<ApiEnvironment<SdlExtensionApi>>);
}  // namespace
}  // namespace vulkandemo::backend
#endif  // DOCTEST_CONFIG_DISABLE
