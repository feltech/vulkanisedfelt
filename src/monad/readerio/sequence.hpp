// IWYU pragma: private, include "../readerio.hpp"
#pragma once
#include <ranges>
#include <tuple>

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

#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>

#include "../../hof/concepts.hpp"
#include "../io/fwd.hpp"
#include "./fwd.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::readerio
{

struct sequence_t
{
	struct io_t
	{
		template <class State>
		struct readerio_to_io_t
		{
			State state;
			constexpr auto operator()(auto && readeriom)
			{
				return FW(readeriom)(state);
			}
		};

		template <std::ranges::range RngOfReaderIOs>
		struct action_t
		{
			RngOfReaderIOs rng_of_readerios;

			constexpr auto operator()(auto const & state)
			{
				using io::IO;
				using RngOfIOs = hof::unspecialise_t<RngOfReaderIOs>::template specialise_t<IO>;
				RngOfIOs rng_of_ios = rng_of_readerios |
					ranges::views::transform(readerio_to_io_t{state}) | ranges::to<RngOfIOs>;
			}
		};

		constexpr auto operator()(std::ranges::range auto && rng_of_readerios) const
		{
			return ReaderIO{action_t{FW(rng_of_readerios)}};
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

auto sequence(hof::specialisation_of<ReaderIO> auto &&... ms)
{
	using boost::hana::ap;
	using boost::hana::append;
	using boost::hana::fold_left;
	using boost::hana::lift;

	return fold_left(
		std::tuple{FW(ms)...},
		lift<readerio_tag_t>(std::tuple{}),
		[](auto && acc, auto && readeriom)
		{
			return ap(
				acc.fmap([](auto && values) { return tuple_appender_t{FW(values)}; }),
				FW(readeriom));
			;
		});
}

auto sequence(std::ranges::range auto && rng)
{
	return sequence_t::io_t{}(FW(rng));
}
}  // namespace vulkandemo::monad::readerio

#include "../../macros_pop.hpp"