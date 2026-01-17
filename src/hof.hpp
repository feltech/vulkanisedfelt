// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#pragma once
#include <ranges>
#include <utility>

#include <range/v3/view/transform.hpp>

#include <immer/vector.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "macros.hpp"

/**
 * Small higher order functions useful for ranges transformations.
 */
namespace vulkandemo::hof
{
template <typename Container>
struct unspecialise_t;

template <template <typename...> class Container, typename... OldArgs>
struct unspecialise_t<Container<OldArgs...>>
{
	template <typename... Args>
	using specialise_t = Container<Args...>;
};

template <
	typename T,
	typename Policy,
	immer::detail::rbts::bits_t B,
	immer::detail::rbts::bits_t BL>
struct unspecialise_t<immer::vector<T, Policy, B, BL>>
{
	template <typename NewT>
	using specialise_t = immer::vector<NewT, Policy, B, BL>;
};

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

template<class T>
struct construct {
	template<class... Args>
	constexpr T operator()(Args&&... args) const
		noexcept(noexcept(T{std::forward<Args>(args)...}))
	{
		return T{std::forward<Args>(args)...};
	}
};

struct transform_concat_t
{
	template <class... Args>
	auto operator()(immer::array<Args...> first, immer::array<Args...>  second) const
	{
		auto out = std::move(first).transient();

		for (auto&& elem : second)
			out.push_back(std::move(elem));

		return out.persistent();
	};

	auto operator()(std::ranges::range auto first, std::ranges::range auto second) const
	{
		first.insert(
			end(first), make_move_iterator(begin(second)), make_move_iterator(end(second)));
		return first;
	};
};

struct transform_range_to_check_non_empty_t
{
	constexpr auto operator()(std::ranges::range auto && values) const
	{
		return !std::ranges::empty(FW(values));
	}
};

struct transform_maybes_to_values_t
{
	template <std::ranges::range InputContainer>
	static constexpr auto operator()(InputContainer && values)
	{
		return FW(values) | std::views::filter([](auto && elem) { return FW(elem).has_value(); }) |
			std::views::transform([](auto && elem) { return *FW(elem); }) |
			ranges::to<unspecialise_t<InputContainer>::template specialise_t>;
	}
};

template <class Second>
struct pair_with_t
{
	Second second;
	constexpr auto operator()(this auto && self, auto && first)
	{
		return std::pair{FW(first), FW(self).second};
	}
};

namespace mem_fn
{
constexpr auto value_of()
{
	// NOLINTNEXTLINE(*-trailing-return)
	return []<typename T>(T && obj) -> decltype(auto) { return std::forward<T>(obj).value_of(); };
}
}  // namespace mem_fn

namespace attr
{
constexpr auto first()
{
	// NOLINTNEXTLINE(*-trailing-return)
	return []<typename T>(T && obj) -> decltype(auto) { return std::forward<T>(obj).first; };
}

constexpr auto second()
{
	// NOLINTNEXTLINE(*-trailing-return)
	return []<typename T>(T && obj) -> decltype(auto) { return std::forward<T>(obj).second; };
}
}  // namespace attr
namespace views
{

template <typename... Args>
constexpr auto value_of(Args &&... args)
{
	return ranges::views::transform(std::forward<Args>(args)..., mem_fn::value_of());
}

template <typename T, typename... Args>
constexpr auto cast(Args &&... args)
{
	return ranges::views::transform(
		std::forward<Args>(args)...,
		// NOLINTNEXTLINE(*-trailing-return)
		[]<typename U>(U && obj) -> decltype(auto)
		{ return static_cast<T>(std::forward<U>(obj)); });
}
}  // namespace views
}  // namespace vulkandemo::hof