#pragma once
// IWYU pragma: private, include "../readerio.hpp"

#include <boost/hana/fwd/chain.hpp>
#include <boost/hana/fwd/lift.hpp>
#include <boost/hana/fwd/transform.hpp>

#include "../io/fwd.hpp"
#include "./fwd.hpp"
#include "../../hof/concepts.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::readerio
{

struct readerio_tag_t;

constexpr auto pure(auto && value)
{
	return boost::hana::lift<readerio_tag_t>(FW(value));
}

constexpr auto lift_io(hof::specialisation_of<io::IO> auto && iom)
{
	return boost::hana::lift<readerio_tag_t>(FW(iom));
}

template <Action Act>
struct ReaderIO
{
	Act action;

	decltype(auto) operator()(this auto && self, auto&& state)
	{
		static_assert(
			hof::specialisation_of<decltype(action(state)), io::IO>,
			"ReaderIO action must return IO");
		return FW(self).action(FW(state));
	}

	[[nodiscard]] auto bind(this auto && self, auto && kleisli)
	{
		return boost::hana::chain(FW(self), FW(kleisli));
	}

	[[nodiscard]] auto fmap(this auto && self, auto && transformer)
	{
		return boost::hana::transform(FW(self), FW(transformer));
	}
};
}  // namespace vulkandemo::monad::readerio

#include "../../macros_pop.hpp"