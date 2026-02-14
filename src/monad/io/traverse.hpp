// IWYU pragma: private, include "../io.hpp"
#pragma once
#include <optional>
#include <ranges>
#include <type_traits>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/transform.hpp>

#include "../detail.hpp"
#include "./fwd.hpp"
// #include "./sequence.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::io
{
struct maybe_t
{
	struct io_t
	{
		template <class Kleisli, class Value>
		struct action_t
		{
			Kleisli kleisli;
			std::optional<Value> value;

			constexpr auto operator()(this auto && self)
			{
				using Ret = std::invoke_result_t<Kleisli, Value>;
				if (!self.value.has_value())
					return std::optional<Ret>{};

				return std::optional<Ret>{FW(self).kleisli(*FW(self).value)()};
			}
		};


		template <class Value>
		static constexpr auto operator()(auto&& kleisli, std::optional<Value> value)
		{
			return IO{action_t{FW(kleisli), std::move(value)}};
		}

		template <class Kleisli>
		struct with_kleisli_t
		{
			Kleisli kleisli;
			template <class Value>
			constexpr auto operator()(this auto && self, std::optional<Value> value)
			{
				return io_t{}(FW(self).kleisli, std::move(value));
			}
		};
	};
};

struct traverse_t
{
	struct io_t
	{
		static constexpr auto operator()(std::ranges::range auto values, auto const & kleisli)
		{
			static_assert(
				std::ranges::range<decltype(values)>, "Traverse expects a range as input");

			using ValueRange = std::decay_t<decltype(values)>;
			using ValueElem = ValueRange::value_type;

			static_assert(
				!hof::specialisation_of<ValueElem, IO>,
				"traverse expects a range of values, not a IOs");

			using IOElem = decltype(kleisli(std::declval<ValueElem>()));
			using IORange = hof::unspecialise_t<ValueRange>::template specialise_t<IOElem>;

			auto ios = values | ranges::views::move | ranges::views::transform(kleisli) |
				ranges::to<IORange>();
			return sequence(std::move(ios));
		}

		template <class Value>
		static constexpr auto operator()(std::optional<Value> value, auto&& kleisli)
		{
			return maybe_t::io_t{}(FW(kleisli), std::move(value));
		}

		template <class Kleisli>
		struct with_kleisli_t
		{
			Kleisli kleisli;
			constexpr auto operator()(this auto && self, std::ranges::range auto && values)
			{
				return io_t{}(FW(values), FW(self).kleisli);
			}
		};
	};
};
}  // namespace vulkandemo::monad::io

#include "../../macros_pop.hpp"