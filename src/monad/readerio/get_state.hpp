#pragma once
// IWYU pragma: private, include "../readerio.hpp"

#include <utility>

#include "../io/fwd.hpp"
#include "./fwd.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::readerio
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
				return FW(self).state;
			}
		};

		static constexpr auto operator()(auto && state)
		{
			return io::IO{action_t{FW(state)}};
		}
	};

	struct readerio_factory_t
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
			return ReaderIO{action_t{}};
		}
	};
};

constexpr auto get_state()
{
	return get_state_t::readerio_factory_t{}();
}
}  // namespace vulkandemo::monad::readerio

#include "../../macros_pop.hpp"
