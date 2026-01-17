// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#pragma once
#include <optional>
#include <ranges>
#include <utility>

#include <range/v3/view/cache1.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/transform.hpp>

#include <immer/array.hpp>
#include <immer/array_transient.hpp>
#include <immer/vector.hpp>

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

template <class T>
struct construct_t
{
	template <class... Args>
	constexpr T operator()(Args &&... args) const noexcept(noexcept(T{std::forward<Args>(args)...}))
	{
		return T{std::forward<Args>(args)...};
	}
};

struct transform_concat_t
{
	template <class... Args>
	auto operator()(immer::array<Args...> first, immer::array<Args...> second) const
	{
		auto out = std::move(first).transient();

		for (auto && elem : second) out.push_back(std::move(elem));

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
	static constexpr auto operator()(std::ranges::range auto && values)
	{
		return !std::ranges::empty(FW(values));
	}
};

struct transform_maybes_to_values_t
{
	template <std::ranges::range InputContainer>
	static constexpr auto operator()(InputContainer && values)
	{
		return values | ranges::views::move |
			ranges::views::filter([](auto const & elem) { return elem.has_value(); }) |
			ranges::views::transform(
				   [](auto & elem) -> decltype(auto) { return std::move(*elem); }) |
			ranges::to<unspecialise_t<InputContainer>::template specialise_t>;
	}
};

struct transform_range_to_front_elem_t
{
	static constexpr auto operator()(auto && values)
	{
		static_assert(std::ranges::range<decltype(values)>, "Expected a range");

		return FW(values).front();
	}
};

template <class Value>
struct transform_bool_to_optional_t
{
	Value value;
	constexpr std::optional<Value> operator()(this auto && self, bool cond)
	{
		return cond ? std::optional{FW(self).value} : std::nullopt;
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

template <std::size_t I>
struct get_nth_t
{
	static constexpr auto operator()(auto && tuple_like)
	{
		return std::get<I>(FW(tuple_like));
	}
};

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