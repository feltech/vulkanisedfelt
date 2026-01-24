#pragma once
// IWYU pragma: private, include "../io.hpp"
#include <boost/hana/fwd/ap.hpp>
#include <boost/hana/fwd/append.hpp>
#include <boost/hana/fwd/fold_left.hpp>
#include <boost/hana/fwd/lift.hpp>

#include "../detail.hpp"
#include "./fwd.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::stateio
{
constexpr auto sequence(hof::specialisation_of<StateIO> auto &&... ms)
{
	using boost::hana::ap;
	using boost::hana::append;
	using boost::hana::fold_left;
	using boost::hana::lift;

	return fold_left(
		std::tuple{FW(ms)...},
		lift<stateio_tag_t>(std::tuple{}),
		[](auto && acc, auto && iom)
		{
			// Applicative - unwrap two IOs, the first yielding a function and the second
			// yielding a value, then call the function with the value in a new IO. In this
			// case, append the result of an io to a tuple.
			return ap(
				// Transform IO result from a tuple of values to a function that appends a value
				// to the (captured) tuple and returns the new tuple.
				acc.fmap(
					[](auto && values)
					{
						return [values = FW(values)](auto && value_to_append)
						{ return append(values, FW(value_to_append)); };
					}),
				FW(iom));
			;
		});
}

}  // namespace vulkandemo::monad::stateio

#include "../../macros_pop.hpp"