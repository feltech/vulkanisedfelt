// IWYU pragma: private, include "../stateio.hpp"
#pragma once
#include <concepts>
#include <utility>

#include <boost/hana/fwd/lift.hpp>

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
				hof::applicable_with<Kleisli, decltype(value)>,
				"Non-StateIO kleisli must be invocable with the StateIO effect value type");
		}
	}
};
};	// namespace vulkandemo::monad::stateio

#include "../../macros_pop.hpp"
