// IWYU pragma: private, include "../io.hpp"
#pragma once
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/transform.hpp>

#include "../detail.hpp"
#include "./fwd.hpp"
#include "./sequence.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::stateio
{
struct maybe_t
{
	struct io_t
	{
		template <class State, class Kleisli, class Value>
		struct action_t
		{
			State state;
			Kleisli kleisli;
			std::optional<Value> value;

			using NewStateIO = std::invoke_result_t<Kleisli, Value>;
			using NewIO = std::invoke_result_t<NewStateIO, State>;
			using NewIOResultAndState = std::invoke_result_t<NewIO>;
			using MaybeNewIOResult = std::optional<std::tuple_element_t<0, NewIOResultAndState>>;
			using NewState = std::tuple_element_t<1, NewIOResultAndState>;
			using Ret = std::pair<MaybeNewIOResult, NewState>;

			constexpr Ret operator()(this auto && self)
			{
				if (!self.value.has_value())
					return std::pair{std::nullopt, FW(self).state};

				auto [new_value, new_state] = FW(self).kleisli(*FW(self).value)(FW(self).state)();
				return std::pair{std::optional{std::move(new_value)}, std::move(new_state)};
			}
		};

		template <class Value>
		static constexpr auto operator()(auto && state, auto && kleisli, std::optional<Value> value)
		{
			return io::IO{action_t{FW(state), FW(kleisli), std::move(value)}};
		}
	};

	struct stateio_t
	{
		template <class Kleisli, class Value>
		struct action_t
		{
			Kleisli kleisli;
			std::optional<Value> value;

			constexpr auto operator()(this auto && self, auto && state)
			{
				return io_t{}(FW(state), FW(self).kleisli, std::move(self.value));
			}
		};

		template <class Value>
		static constexpr auto operator()(auto && kleisli, std::optional<Value> value)
		{
			// Ideally we could either call the kleisli or not depending on value, and if not
			// return some kind of identity/null StateIO. However, every StateIO has a unique
			// type (due to template shenanigans) so we must wrap everything in a single custom
			// IO in order to have a consistent type.
			return StateIO{action_t{FW(kleisli), std::move(value)}};
		}

		template <class Kleisli>
		struct with_kleisli_t
		{
			Kleisli kleisli;
			template <class Value>
			constexpr auto operator()(this auto && self, std::optional<Value> value)
			{
				return stateio_t{}(FW(self).kleisli, std::move(value));
			}
		};
	};
};

struct traverse_t
{
	struct stateio_t
	{
		static constexpr auto operator()(std::ranges::range auto values, auto const & kleisli)
		{
			static_assert(
				std::ranges::range<decltype(values)>, "Traverse expects a range as input");

			using ValueRange = std::decay_t<decltype(values)>;
			using ValueElem = ValueRange::value_type;

			static_assert(
				!hof::specialisation_of<ValueElem, StateIO>,
				"traverse expects a range of values, not StateIOs");

			using MElem = decltype(kleisli(std::declval<ValueElem>()));
			using MRange = hof::unspecialise_t<ValueRange>::template specialise_t<MElem>;

			auto ios = values | ranges::views::move | ranges::views::transform(kleisli) |
				ranges::to<MRange>();
			return sequence(std::move(ios));
		}

		template <class Value>
		static constexpr auto operator()(std::optional<Value> value, auto && kleisli)
		{
			return maybe_t::stateio_t{}(FW(kleisli), std::move(value));
		}

		template <class Kleisli>
		struct with_kleisli_t
		{
			Kleisli kleisli;
			constexpr auto operator()(this auto && self, auto && values)
			{
				return stateio_t{}(FW(values), FW(self).kleisli);
			}
		};
	};
};
}  // namespace vulkandemo::monad::stateio

#include "../../macros_pop.hpp"