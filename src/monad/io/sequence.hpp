#pragma once
#include <tuple>
#include <type_traits>

#include <boost/hana/fwd/ap.hpp>
#include <boost/hana/fwd/append.hpp>
#include <boost/hana/fwd/fold_left.hpp>
#include <boost/hana/fwd/lift.hpp>

// Headers technically not necessary here, but consumer must include them.
// NOLINTBEGIN(*-include-cleaner)
#include <boost/hana/ap.hpp>
#include <boost/hana/append.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/fold_left.hpp>
#include <boost/hana/lift.hpp>
// NOLINTEND(*-include-cleaner)

#include <libfork/core.hpp>
#include <libfork/core/task.hpp>

#include "../detail.hpp"
#include "./fwd.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::io
{
struct io_tag_t;

struct sequence_t
{
	struct io_factory_t
	{
		template <class Arg>
		struct async_function_t : detail::AsyncFunctorInterface<Arg>
		{
			using RngOfIOs = Arg;
			using IOElem = RngOfIOs::value_type;
			using IOElemResult = std::invoke_result_t<IOElem>;
			using IOElemValue = detail::unwrap_async_t<IOElemResult>;
			using Rng = hof::unspecialise_t<RngOfIOs>::template specialise_t<IOElemValue>;

			static_assert(!hof::specialisation_of<IOElemValue, IO>, "IO effect/return is IO");

			using IOResultType = Rng;

			static constexpr auto fn = []([[maybe_unused]] auto fn,
										  RngOfIOs rng_of_ios) -> lf::task<Rng>
			{
				static constexpr bool kIsIOAsync = detail::unwrap_async_v<IOElemResult>;

				std::vector<IOElemValue> outputs(rng_of_ios.size());

				for (std::size_t idx = 0; idx < rng_of_ios.size(); ++idx)
				{
					auto const & iom = rng_of_ios[idx];

					if constexpr (kIsIOAsync)
					{
						auto async_fn = iom();
						co_await lf::fork[&outputs[idx], async_fn.fn](std::move(async_fn).arg);
					}
					else
					{
						co_await lf::fork[&outputs[idx], lf::lift](iom);
					}
				}
				co_await lf::join;

				Rng ret(make_move_iterator(outputs.begin()), make_move_iterator(outputs.end()));

				co_return ret;
			};
		};
		template <std::ranges::range RngOfIOs>
		async_function_t(RngOfIOs) -> async_function_t<RngOfIOs>;

		template <std::ranges::range RngOfIOs>
		struct action_t
		{
			RngOfIOs rng_of_ios;

			constexpr auto operator()(this auto && self)
			{
				return async_function_t{FW(self).rng_of_ios};
			}
		};

		constexpr auto operator()(std::ranges::range auto && rng_of_ios) const
		{
			return IO{action_t{FW(rng_of_ios)}};
		}
	};
};

template <typename Tuple>
struct tuple_appender_t
{
	Tuple vals;
	constexpr auto operator()(this auto && self, auto && value_to_append)
	{
		using boost::hana::append;
		return append(FW(self).vals, FW(value_to_append));
	}
};

auto sequence(hof::specialisation_of<IO> auto &&... ms)
{
	using boost::hana::ap;
	using boost::hana::append;
	using boost::hana::fold_left;
	using boost::hana::lift;

	return fold_left(
		std::tuple{FW(ms)...},
		lift<io_tag_t>(std::tuple{}),
		[](auto && acc, auto && iom)
		{
			// Applicative - unwrap two IOs, the first yielding a function and the second
			// yielding a value, then call the function with the value in a new IO. In this
			// case, append the result of an io to a tuple.
			return ap(
				// Transform IO result from a tuple of values to a function that appends a value
				// to the (captured) tuple and returns the new tuple.
				acc.fmap([](auto && values) { return tuple_appender_t{FW(values)}; }),
				FW(iom));
			;
		});
}

auto sequence(std::ranges::range auto && rng)
{
	return sequence_t::io_factory_t{}(FW(rng));
}
}  // namespace vulkandemo::monad::io

#include "../../macros_pop.hpp"