#pragma once
// IWYU pragma: private, include "../io.hpp"

#include <type_traits>
#include "../../hof.hpp"
#include "../detail.hpp"
#include "./traverse.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::io
{

struct filter_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(auto && values, auto && kleisli)
		{
			using Rng = std::decay_t<decltype(values)>;
			using Elem = Rng::value_type;
			using IOElem = std::decay_t<decltype(kleisli(std::declval<Elem>()))>;
			using IOElemResult = std::invoke_result_t<IOElem>;
			using IOElemValue = detail::unwrap_async_t<IOElemResult>;
			static_assert(
				std::is_same_v<IOElemValue, std::optional<Elem>>,
				"filter expects a kleisli that returns an IO whose effect is a std::optional of "
				"the input element");

			Rng filterable_values = values;
			return traverse_t::io_factory_t{}(FW(values), FW(kleisli))
				.fmap(hof::transform_maybes_to_values_t{});
		}

		template <class Kleisli>
		struct with_kleisli_t
		{
			Kleisli kleisli;
			constexpr auto operator()(this auto && self, std::ranges::range auto && values)
			{
				return io_factory_t{}(FW(values), FW(self).kleisli);
			}
		};
	};
};
}  // namespace vulkandemo::monad::io

#include "../../macros_pop.hpp"