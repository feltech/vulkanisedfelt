#pragma once

#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/fwd/ap.hpp>
#include <boost/hana/fwd/chain.hpp>
#include <boost/hana/fwd/lift.hpp>
#include <boost/hana/fwd/transform.hpp>
#include <concepts>
#include <utility>

#include "../detail.hpp"
#include "../io/fwd.hpp"
#include "./fwd.hpp"

#include "../../macros_push.hpp"

namespace boost::hana
{
namespace io = vulkandemo::monad::io;
namespace stateio = vulkandemo::monad::stateio;
using vulkandemo::monad::detail::applicable_with;
using vulkandemo::monad::detail::specialisation_of;
using vulkandemo::monad::detail::Transformer;
using vulkandemo::monad::detail::tuple_like;

template <typename A>
struct tag_of<stateio::StateIO<A>>
{
	using type = stateio::stateio_tag_t;
};

template <>
struct lift_impl<stateio::stateio_tag_t>
{
	static auto apply(specialisation_of<io::IO> auto && iom)
	{
		return stateio::StateIO{action_t{FW(iom)}};
	}

	static auto apply(auto && value)
	{
		return stateio::StateIO{action_t{lift<io::io_tag_t>(FW(value))}};
	}

	template <class IO>
	struct action_t
	{
		IO iom;

		constexpr auto operator()(auto && state) const
		{
			return iom.fmap(vulkandemo::hof::pair_with_t{FW(state)});
		}
	};
};

template <>
struct chain_impl<stateio::stateio_tag_t>
{
	static constexpr auto apply(auto && stateiom, auto && lifter)
	{
		return stateio::StateIO{stateio_action_t{FW(stateiom), FW(lifter)}};
	}

	template <class WrappedStateIO, class StateIOKleisli>
	struct stateio_action_t
	{
		WrappedStateIO stateiom;
		StateIOKleisli kleisli;
		constexpr auto operator()(this auto && self, auto && state)
		{
			// NOLINTNEXTLINE(bugprone-use-after-move)
			return FW(self).stateiom(FW(state)).bind(io_lifter_t{FW(self).kleisli});
		}
	};

	template <class StateIOKleisli>
	struct io_lifter_t
	{
		StateIOKleisli kleisli;
		constexpr auto operator()(this auto && self, auto && value_and_state)
		{
			if constexpr (std::invocable<StateIOKleisli, decltype(value_and_state.first)>)
			{
				auto new_stateio = FW(self).kleisli(FW(value_and_state).first);
				static_assert(
					specialisation_of<decltype(new_stateio), stateio::StateIO>,
					"StateIO lifter must return a StateIO");
				auto new_io = std::move(new_stateio)(std::move(value_and_state.second));
				static_assert(
					specialisation_of<decltype(new_io), io::IO>,
					"StateIO action must return an IO");
				return new_io;
			}
			else if constexpr (applicable_with<StateIOKleisli, decltype(value_and_state.first)>)
			{
				auto new_stateio = std::apply(FW(self).kleisli, FW(value_and_state).first);
				static_assert(
					specialisation_of<decltype(new_stateio), stateio::StateIO>,
					"StateIO lifter must return a StateIO");
				auto new_io = std::move(new_stateio)(FW(value_and_state).second);
				static_assert(
					specialisation_of<decltype(new_io), io::IO>,
					"StateIO action must return an IO");
				return new_io;
			}
			else
			{
				static_assert(
					std::invocable<StateIOKleisli, decltype(value_and_state.first)>,
					"StateIO kleisli not compatible with value.");
			}
		}
	};
};

template <>
struct transform_impl<stateio::stateio_tag_t>
{
	static constexpr auto apply(auto && stateiom, auto && transformer)
	{
		return stateio::StateIO{stateio_action_t{FW(stateiom), FW(transformer)}};
	}

	template <class WrappedStateIO, class Transformer>
	struct stateio_action_t
	{
		WrappedStateIO stateiom;
		Transformer transformer;

		constexpr auto operator()(this auto && self, auto && state)
		{
			return FW(self).stateiom(state).fmap(io_transformer_t{FW(self).transformer, FW(state)});
		}
	};

	template <class State, class ValueTransformer>
	struct io_transformer_t
	{
		ValueTransformer transformer;
		State state;

		constexpr auto operator()(this auto && self, auto && value_and_state)
		{
			if constexpr (std::invocable<ValueTransformer, decltype(value_and_state.first)>)
			{
				return std::pair{
					FW(self).transformer(FW(value_and_state).first), FW(value_and_state).second};
			}
			else if constexpr (applicable_with<ValueTransformer, decltype(value_and_state.first)>)
			{
				return std::pair{
					std::apply(FW(self).transformer, FW(value_and_state).first),
					FW(value_and_state).second};
			}
			else
			{
				static_assert(
					std::invocable<ValueTransformer, decltype(value_and_state.first)>,
					"StateIO value transformer is not callable with value");
			}
		}
	};
};

template <>
struct ap_impl<stateio::stateio_tag_t>
{
	// Note that StateIO is inherently sequential since state must be threaded
	// through, so may as well use nested bind() calls (unlike e.g. ap<io_tag_t>).
	static constexpr auto apply(auto && lhs, auto && rhs)
	{
		return FW(lhs).bind(fn_kleisli_t{FW(rhs)});
	}

	template <class ValueStateIO>
	struct fn_kleisli_t
	{
		ValueStateIO value_stateio;

		constexpr auto operator()(this auto && self, auto && fn)
		{
			return FW(self).value_stateio.bind(value_kleisli_t{FW(fn)});
		}
	};

	template <class Fn>
	struct value_kleisli_t
	{
		Fn fn;
		constexpr auto operator()(this auto && self, auto && value)
		{
			return lift<stateio::stateio_tag_t>(FW(self).fn(FW(value)));
		}
	};
};
}  // namespace boost::hana

#include "../../macros_pop.hpp"