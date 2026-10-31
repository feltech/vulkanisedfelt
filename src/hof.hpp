// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell

/**
 * @file hof.hpp
 *
 * Higher-order utilities for composing functions.
 */

#pragma once
#include <utility>

#include <range/v3/view/transform.hpp>

/**
 * Small higher order functions useful for ranges transformations.
 */
namespace vulkandemo::hof
{

/**
 * Returns a lambda that performs a static_cast to the given type.
 *
 * @tparam T The type to cast to.
 *
 * @return A lambda that takes any object and returns it cast to T.
 */
template <typename T>
constexpr auto cast()
{
    // NOLINTNEXTLINE(*-trailing-return)
    return []<typename U>(U && obj) -> decltype(auto)
    { return static_cast<T>(std::forward<U>(obj)); };
};

namespace mem_fn
{
/**
 * Unwrap strong types through their value_of() member.
 *
 * @return A callable that forwards its argument's underlying value.
 */
constexpr auto value_of()
{
    // NOLINTNEXTLINE(*-trailing-return)
    return []<typename T>(T && obj) -> decltype(auto) { return std::forward<T>(obj).value_of(); };
}
}  // namespace mem_fn

namespace attr
{
/**
 * Select the first member of pair-like objects.
 *
 * @return A callable that forwards its argument's first member.
 */
constexpr auto first()
{
    // NOLINTNEXTLINE(*-trailing-return)
    return []<typename T>(T && obj) -> decltype(auto) { return std::forward<T>(obj).first; };
}

/**
 * Select the second member of pair-like objects.
 *
 * @return A callable that forwards its argument's second member.
 */
constexpr auto second()
{
    // NOLINTNEXTLINE(*-trailing-return)
    return []<typename T>(T && obj) -> decltype(auto) { return std::forward<T>(obj).second; };
}
}  // namespace attr
namespace views
{

/**
 * Adapt a range of strong types to a view of their underlying values.
 *
 * @param args Arguments forwarded to the transform view adaptor.
 * @return A value-unwrapping view or adaptor closure.
 */
template <typename... Args>
constexpr auto value_of(Args &&... args)
{
    return ranges::views::transform(std::forward<Args>(args)..., mem_fn::value_of());
}
}  // namespace views
}  // namespace vulkandemo::hof
