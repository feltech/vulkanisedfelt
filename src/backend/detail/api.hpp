// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file backend/detail/api.hpp
 *
 * Borrowed API capabilities for ReaderIO effects.
 */
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>

namespace vulkandemo::backend::detail
{
/** An environment borrowing its command implementation through a reference wrapper. */
template <class TEnvironment>
concept api_environment = requires(TEnvironment const & environment)
{
    typename std::remove_cvref_t<decltype(environment.api)>::type;
    requires std::same_as<
        std::remove_cvref_t<decltype(environment.api)>,
        std::reference_wrapper<typename std::remove_cvref_t<decltype(environment.api)>::type>>;
};
}  // namespace vulkandemo::backend::detail
