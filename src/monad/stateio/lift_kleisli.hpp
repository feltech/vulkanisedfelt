// IWYU pragma: private, include "../stateio.hpp"
#pragma once
#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/hana/fwd/lift.hpp>
#include <boost/hana/fwd/zip.hpp>
// #include <boost/hana/zip.hpp>

#include "../../hof/concepts.hpp"
#include "./fwd.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::stateio
{
template <class Kleisli>
struct lift_kleisli_t
{
	Kleisli kleisli;

	constexpr auto operator()(this auto && self, auto && value)
	{
		if constexpr (std::invocable<Kleisli, decltype(value)>)
		{
			return boost::hana::lift<stateio_tag_t>(FW(self).kleisli(FW(value)));
		}
		else if constexpr (hof::applicable_with<Kleisli, decltype(value)>)
		{
			return boost::hana::lift<stateio_tag_t>(std::apply(FW(self).kleisli, FW(value)));
		}
		else
		{
			static_assert(
				std::invocable<Kleisli, decltype(value)>,
				"Non-StateIO kleisli must be invocable with the StateIO effect value type");
		}
	}
};

template <class... Kleislis>
struct fanout_t
{
	std::tuple<Kleislis...> kleislis;

	constexpr auto operator()(this auto && self, auto && value)
	{
		return sequence(
			boost::hana::transform(
				FW(self).kleislis,
				[&](auto && kleisli)
				{
					using Kleisli = std::decay_t<decltype(kleisli)>;
					if constexpr (std::invocable<Kleisli, decltype(value)>)
					{
						return FW(kleisli)(value);
					}
					else if constexpr (hof::applicable_with<Kleisli, decltype(value)>)
					{
						return std::apply(FW(kleisli), value);
					}
					else
					{
						static_assert(
							std::invocable<Kleisli, decltype(value)>,
							"StateIO kleisli must be invocable with the StateIO effect value type");
					}
				}));
	}
};

constexpr auto fanout(auto &&... kleislis)
{
	return fanout_t{std::tuple{kleislis...}};
}

template <class... Kleislis>
struct split_t
{
	std::tuple<Kleislis...> kleislis;

	template <class... Value>
	constexpr auto operator()(this auto && self, std::tuple<Value...> values)
	{
		return sequence(boost::hana::transform(
			boost::hana::zip(FW(self).kleislis, std::move(values)),
			[](auto && k_and_v)
			{
				auto && [kleisli, value] = k_and_v;

				using Kleisli = std::decay_t<decltype(kleisli)>;
				if constexpr (std::invocable<Kleisli, decltype(value)>)
				{
					return FW(kleisli)(FW(value));
				}
				else if constexpr (hof::applicable_with<Kleisli, decltype(value)>)
				{
					return std::apply(FW(kleisli), FW(value));
				}
				else
				{
					static_assert(
						std::invocable<Kleisli, decltype(value)>,
						"StateIO kleisli must be invocable with the StateIO effect value type");
				}
			}));
	}
};

constexpr auto split(auto &&... kleislis)
{
	return split_t{std::tuple{kleislis...}};
}

};	// namespace vulkandemo::monad::stateio

#include "../../macros_pop.hpp"
