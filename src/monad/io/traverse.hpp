#pragma once
#include <ranges>
#include <type_traits>

#include <range/v3/to_container.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/transform.hpp>

#include "../detail.hpp"
#include "./fwd.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::io
{
struct traverse_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(auto values, auto const & kleisli)
		{
			static_assert(
				std::ranges::range<decltype(values)>, "Traverse expects a range as input");

			using ValueRange = std::decay_t<decltype(values)>;
			using ValueElem = ValueRange::value_type;

			static_assert(
				!detail::specialisation_of<ValueElem, IO>,
				"traverse expects a range of values, not a IOs");

			using IOElem = decltype(kleisli(std::declval<ValueElem>()));
			using IORange = detail::Unspecialise<ValueRange>::template Specialise<IOElem>;

			auto ios = values | ranges::views::move | ranges::views::transform(kleisli) |
				ranges::to<IORange>();
			return sequence(std::move(ios));
		}

		template <class ElementLifter>
		struct with_kleisli_t
		{
			ElementLifter kliesli;
			constexpr auto operator()(this auto && self, std::ranges::range auto && values)
			{
				return io_factory_t{}(FW(values), FW(self).kliesli);
			}
		};
	};
};
}  // namespace vulkandemo::monad::io

#include "../../macros_pop.hpp"