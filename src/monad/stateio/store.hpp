// IWYU pragma: private, include "../stateio.hpp"
#pragma once

#include <utility>

#include "../io/fwd.hpp"
#include "./fwd.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::stateio
{
struct store_t
{
	struct io_factory_t
	{
		template <class State, class Value, class Fn>
		struct action_t
		{
			State state;
			Value value;
			Fn fn;

			constexpr auto operator()(this auto && self)
			{
				auto value_to_return = self.value;
				std::pair result{
					std::move(value_to_return), FW(self).fn(FW(self).value, FW(self).state)};
				return result;
			}
		};

		static constexpr auto operator()(auto && state, auto && value, auto && fn)
		{
			return io::IO{action_t{FW(state), FW(value), FW(fn)}};
		}
	};

	struct stateio_factory_t
	{
		template <class Value, class Fn>
		struct action_t
		{
			Value value;
			Fn fn;
			constexpr auto operator()(this auto && self, auto && state)
			{
				return io_factory_t{}(FW(state), FW(self).value, FW(self).fn);
			}
		};

		static constexpr auto operator()(auto && value, auto && fn)
		{
			return StateIO{action_t{FW(value), FW(fn)}};
		}

		template <class Fn>
		struct with_mutator_t
		{
			Fn fn;
			constexpr auto operator()(this auto && self, auto && value)
			{
				return stateio_factory_t{}(FW(value), FW(self).fn);
			}
		};
	};
};
}  // namespace vulkandemo::monad::stateio

#include "../../macros_pop.hpp"
