#pragma once
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/fwd/ap.hpp>
#include <boost/hana/fwd/append.hpp>
#include <boost/hana/fwd/chain.hpp>
#include <boost/hana/fwd/fold_left.hpp>
#include <boost/hana/fwd/lift.hpp>

#include <libfork/algorithm/lift.hpp>
#include <libfork/core.hpp>
#include <libfork/core/control_flow.hpp>
#include <libfork/core/eventually.hpp>
#include <libfork/core/just.hpp>
#include <libfork/core/scheduler.hpp>
#include <libfork/core/task.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include <boost/hana.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/fwd/core/to.hpp>
#include <boost/hana/fwd/transform.hpp>

#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <immer/detail/rbts/bits.hpp>
#include <immer/vector.hpp>

#include "./monad/detail.hpp"
#include "./monad/io/IO.hpp"
#include "./monad/io/filter.hpp"
#include "./monad/io/fwd.hpp"
#include "./monad/io/hana.hpp"
#include "./monad/io/sequence.hpp"
#include "./monad/io/traverse.hpp"
#include "./monad/stateio/fwd.hpp"
#include "./monad/stateio/store.hpp"
#include "./monad/stateio/hana.hpp"
#include "hof.hpp"

#include "macros_push.hpp"

namespace vulkandemo::monad::stateio
{
// NOLINTBEGIN(*-overloaded-operator,*-trailing-return,*-identifier-length)

template <typename Act>
struct StateIO
{
	Act action;
	// using Ret = typename detail::FnTraits<Act>::return_value;

	decltype(auto) operator()(this auto && self, auto && state)
	{
		// Must return an IO monad that itself returns a pair.
		static_assert(
			detail::specialisation_of<decltype(action(state)), io::IO>,
			// && detail::specialisation_of<decltype(action(state)()), std::pair>,
			"StateIO action must return IO<pair<value, state>>");

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

	auto then(this auto && self, detail::specialisation_of<StateIO> auto && stateiom)
	{
		return FW(self).bind([stateiom = FW(stateiom)]([[maybe_unused]] auto &&... unused)
							 { return stateiom; });
	}

	auto store(this auto && self, auto && fn)
	{
		return FW(self).bind(store_t::stateio_factory_t::with_mutator_t{FW(fn)});
	}
};

struct get_state_t
{
	struct io_factory_t
	{
		template <class State>
		struct action_t
		{
			State state;
			constexpr auto operator()(this auto && self)
			{
				auto value = self.state;
				return std::pair{std::move(value), FW(self).state};
			}
		};

		static constexpr auto operator()(auto && state)
		{
			return io::IO{action_t{FW(state)}};
		}
	};

	struct stateio_factory_t
	{
		struct action_t
		{
			static constexpr auto operator()(auto && state)
			{
				return io_factory_t{}(FW(state));
			}
		};

		static constexpr auto operator()()
		{
			return StateIO{action_t{}};
		}
	};
};

constexpr auto get_state()
{
	return get_state_t::stateio_factory_t{}();
}

constexpr auto pure(auto && value)
{
	return boost::hana::lift<stateio_tag_t>(FW(value));
}

constexpr auto liftIO(detail::specialisation_of<io::IO> auto && iom)
{
	return boost::hana::lift<stateio_tag_t>(FW(iom));
}

auto sequence(detail::specialisation_of<StateIO> auto &&... ms)
{
	using boost::hana::ap;
	using boost::hana::append;
	using boost::hana::fold_left;

	return fold_left(
		std::tuple{FW(ms)...},
		pure(std::tuple{}),
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

} // namespace vulkandemo::monad::stateio

// NOLINTEND(*-overloaded-operator,*-trailing-return,*-identifier-length)

