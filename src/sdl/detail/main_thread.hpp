// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file sdl/detail/main_thread.hpp
 *
 * Main-thread dispatch capabilities for SDL effects.
 */
#pragma once

#include <concepts>

#include <libfork/core/ext/context.hpp>

#include "../../backend/detail/api.hpp"

namespace vulkandemo::sdl
{
/** Main-thread dispatch and error reporting for SDL effects. */
template <class TEnvironment>
concept main_thread_environment =
    backend::api_environment<TEnvironment> && requires(TEnvironment environment)
{
    {environment.main_context}->std::convertible_to<lf::worker_context *>;
    {environment.api.get().sdl_get_error()}->std::same_as<char const *>;
};
}  // namespace vulkandemo::sdl
