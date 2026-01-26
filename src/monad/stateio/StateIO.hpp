// IWYU pragma: private, include "../stateio.hpp"
#pragma once

#include <boost/hana/fwd/chain.hpp>
#include <boost/hana/fwd/lift.hpp>
#include <boost/hana/fwd/transform.hpp>

#include "../io/fwd.hpp"
#include "../readerio/fwd.hpp"
#include "./fwd.hpp"
#include "./store.hpp"
#include "../../hof/concepts.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::stateio
{

constexpr auto pure(auto && value)
{
	return boost::hana::lift<stateio_tag_t>(FW(value));
}

constexpr auto lift_io(hof::specialisation_of<io::IO> auto && iom)
{
	return boost::hana::lift<stateio_tag_t>(FW(iom));
}

constexpr auto lift_readerio(hof::specialisation_of<readerio::ReaderIO> auto && iom)
{
	return boost::hana::lift<stateio_tag_t>(FW(iom));
}

template <typename Act>
struct StateIO
{
	Act action;
	// using Ret = typename detail::FnTraits<Act>::return_value;

	decltype(auto) operator()(this auto && self, auto && state)
	{
		// Must return an IO monad that itself returns a pair.
		static_assert(
			hof::specialisation_of<decltype(action(state)), io::IO>,
			"StateIO action must return IO");

		return FW(self).action(FW(state));
	}

	auto bind(this auto && self, auto && lifter)
	{
		return boost::hana::chain(FW(self), FW(lifter));
	}

	auto fmap(this auto && self, auto && transformer)
	{
		return boost::hana::transform(FW(self), FW(transformer));
	}

	template <class OtherStateIO>
	struct then_t
	{
		OtherStateIO next;

		constexpr auto operator()(this auto && self, [[maybe_unused]] auto && ignored)
		{
			return FW(self).next;
		}
	};

	auto then(this auto && self, hof::specialisation_of<StateIO> auto && stateiom)
	{
		return FW(self).bind(then_t{FW(stateiom)});
	}

	auto store(this auto && self, auto && fn)
	{
		return FW(self).bind(store_t::stateio_factory_t::with_mutator_t{FW(fn)});
	}
};
}  // namespace vulkandemo::monad::stateio

#include "../../macros_pop.hpp"