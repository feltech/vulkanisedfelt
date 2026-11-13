// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/instance_extensions.hpp
 *
 * Main-thread retrieval of SDL's required Vulkan instance extensions.
 */
#pragma once

#include <concepts>
#include <functional>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <SDL.h>
#include <fnfelt/monad/io.hpp>
#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "../../text/string.hpp"
#include "../errors.hpp"
#include "../main_thread.hpp"
#include "./errors.hpp"
#include "./main_thread.hpp"

namespace vulkandemo::sdl
{
/** Commands needed to discover a window's required Vulkan instance extensions. */
template <class TEnvironment>
concept sdl_vulkan_get_instance_extensions_environment =
    main_thread_environment<TEnvironment> &&
    requires(
        TEnvironment environment, SDL_Window * window, unsigned int * count, char const ** names)
{
    {environment.api.get().sdl_vulkan_get_instance_extensions(window, count, names)}
        ->std::same_as<SDL_bool>;
};
}  // namespace vulkandemo::sdl

namespace vulkandemo::sdl::detail
{
/**
 * Retrieves an owned snapshot of the window's required Vulkan extensions.
 *
 * @tparam TApi Command implementation type.
 * @param api Borrowed command implementation.
 * @param window Borrowed Vulkan-capable window.
 * @return Owned extension names, or the immediate SDL failure.
 */
template <class TApi>
auto get_instance_extensions(std::reference_wrapper<TApi> api, SDL_Window * window)
    -> sdl_result<immer::array<text::String>> pre(is_main_thread())
{
    namespace result = fnfelt::result;
    using extension_names = immer::array<text::String>;
    using result_type = sdl_result<extension_names>;

    unsigned int count = 0;
    if (api.get().sdl_vulkan_get_instance_extensions(window, &count, nullptr) == SDL_FALSE)
    {
        return result::error<result_type>(
            last_failure(api, FallibleSdlOperation::enumerate_instance_extensions));
    }
    if (count == 0)
    {
        return result::success<result_type>(extension_names{});
    }

    // Local pointer storage receives SDL's borrowed strings for this snapshot.
    auto const capacity = count;
    std::vector<char const *> names(capacity);
    if (api.get().sdl_vulkan_get_instance_extensions(window, &count, names.data()) == SDL_FALSE)
    {
        return result::error<result_type>(
            last_failure(api, FallibleSdlOperation::enumerate_instance_extensions));
    }
    contract_assert(count <= capacity);

    // Owned name bytes remain valid after SDL changes or releases its storage.
    auto extensions = extension_names(count).transient();
    auto output = std::span{extensions.data_mut(), count};
    for (auto [name, extension] : std::views::zip(std::span{names}.first(count), output))
    {
        extension = text::String{name};
    }
    return result::success<result_type>(std::move(extensions).persistent());
}

/**
 * Produces a main-thread extension query from an execution environment.
 *
 * @tparam TEnvironment Execution capabilities.
 * @param window Borrowed Vulkan-capable window.
 * @param environment Borrowed command implementation and main-thread context.
 * @return IO producing owned required extension names or an SDL failure.
 */
template <class TEnvironment>
constexpr auto io_get_instance_extensions(SDL_Window * window, TEnvironment environment)
{
    using fnfelt::literals::operator""_ss;
    namespace io = fnfelt::monad::io;
    using api_type = typename std::remove_cvref_t<decltype(environment.api)>::type;

    // Callable providing an owned snapshot while SDL's borrowed strings are alive.
    auto get_extensions =
        std::bind_front(get_instance_extensions<api_type>, environment.api, window);

    // IO providing the snapshot and error capture on the main thread.
    return io::on<"sdl_instance_extensions_on_main"_ss>(
        environment.main_context,
        io::create<"sdl_vulkan_get_instance_extensions"_ss>(std::move(get_extensions)));
}
}  // namespace vulkandemo::sdl::detail
