#pragma once
// IWYU pragma: private, include "../io.hpp"

#include <utility>

#include "../io/fwd.hpp"
#include "./fwd.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::stateio
{
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
}  // namespace vulkandemo::monad::stateio

#include "../../macros_pop.hpp"
