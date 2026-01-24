#pragma once
// IWYU pragma: private, include "../io.hpp"
#include <concepts>
#include <utility>

#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/fwd/ap.hpp>
#include <boost/hana/fwd/chain.hpp>
#include <boost/hana/fwd/lift.hpp>
#include <boost/hana/fwd/transform.hpp>

#include "../../hof/concepts.hpp"
#include "../detail.hpp"
#include "../io/fwd.hpp"
#include "./fwd.hpp"

#include "../../macros_push.hpp"

namespace boost::hana
{
namespace io = vulkandemo::monad::io;
namespace readerio = vulkandemo::monad::readerio;
using vulkandemo::hof::applicable_with;
using vulkandemo::hof::specialisation_of;
using vulkandemo::monad::detail::Transformer;

template <typename A>
struct tag_of<readerio::ReaderIO<A>>
{
	using type = readerio::readerio_tag_t;
};

template <>
struct lift_impl<readerio::readerio_tag_t>
{
	static auto apply(specialisation_of<io::IO> auto && iom)
	{
		return readerio::ReaderIO{action_t{FW(iom)}};
	}

	static auto apply(auto && value)
	{
		return readerio::ReaderIO{action_t{lift<io::io_tag_t>(FW(value))}};
	}

	template <class IO>
	struct action_t
	{
		IO iom;

		constexpr auto operator()(this auto && self, [[maybe_unused]] auto && state)
		{
			return FW(self).iom;
		}
	};
};

template <>
struct chain_impl<readerio::readerio_tag_t>
{
	static constexpr auto apply(auto && readeriom, auto && lifter)
	{
		return readerio::ReaderIO{readerio_action_t{FW(readeriom), FW(lifter)}};
	}

	template <class WrappedReaderIO, class ReaderIOKleisli>
	struct readerio_action_t
	{
		WrappedReaderIO source;
		ReaderIOKleisli kleisli;
		constexpr auto operator()(this auto && self, auto && state)
		{
			auto state_for_source = state;
			// NOLINTNEXTLINE(bugprone-use-after-move)
			auto iom = FW(self).source(std::move(state_for_source));
			return std::move(iom).bind(io_lifter_t{FW(state), FW(self).kleisli});
		}
	};

	template <class State, class ReaderIOKleisli>
	struct io_lifter_t
	{
		State state;
		ReaderIOKleisli kleisli;

		constexpr auto operator()(this auto && self, auto && value)
		{
			if constexpr (std::invocable<ReaderIOKleisli, decltype(value)>)
			{
				auto new_readerio = FW(self).kleisli(FW(value));
				static_assert(
					specialisation_of<decltype(new_readerio), readerio::ReaderIO>,
					"ReaderIO lifter must return a ReaderIO");
				auto new_io = std::move(new_readerio)(FW(self).state);
				static_assert(
					specialisation_of<decltype(new_io), io::IO>,
					"ReaderIO action must return an IO");
				return new_io;
			}
			else if constexpr (applicable_with<ReaderIOKleisli, decltype(value)>)
			{
				auto new_readerio = std::apply(FW(self).kleisli, FW(value));
				static_assert(
					specialisation_of<decltype(new_readerio), readerio::ReaderIO>,
					"ReaderIO lifter must return a ReaderIO");
				auto new_io = std::move(new_readerio)(FW(self).state);
				static_assert(
					specialisation_of<decltype(new_io), io::IO>,
					"ReaderIO action must return an IO");
				return new_io;
			}
			else
			{
				static_assert(
					std::invocable<ReaderIOKleisli, decltype(value)>,
					"ReaderIO kleisli not compatible with value.");
			}
		}
	};
};

template <>
struct transform_impl<readerio::readerio_tag_t>
{
	static constexpr auto apply(auto && readeriom, auto && transformer)
	{
		return readerio::ReaderIO{readerio_action_t{FW(readeriom), FW(transformer)}};
	}

	template <class WrappedReaderIO, class Transformer>
	struct readerio_action_t
	{
		WrappedReaderIO source;
		Transformer transformer;

		constexpr auto operator()(this auto && self, auto && state)
		{
			return FW(self).source(FW(state)).fmap(io_transformer_t{FW(self).transformer});
		}
	};

	template <class ValueTransformer>
	struct io_transformer_t
	{
		ValueTransformer transformer;

		constexpr auto operator()(this auto && self, auto && value)
		{
			if constexpr (std::invocable<ValueTransformer, decltype(value)>)
			{
				return FW(self).transformer(FW(value));
			}
			else if constexpr (applicable_with<ValueTransformer, decltype(value)>)
			{
				return std::apply(FW(self).transformer, FW(value));
			}
			else
			{
				static_assert(
					std::invocable<ValueTransformer, decltype(value)>,
					"ReaderIO value transformer is not callable with value");
			}
		}
	};
};

template <>
struct ap_impl<readerio::readerio_tag_t>
{
	static constexpr auto apply(auto && fn_source, auto && value_source)
	{
		return readerio::ReaderIO{readerio_action_t{FW(fn_source), FW(value_source)}};
	}

	template <class FnReaderIO, class ValueReaderIO>
	struct readerio_action_t
	{
		FnReaderIO fn_readerio;
		ValueReaderIO value_readerio;
		constexpr auto operator()(this auto && self, auto && state)
		{
			auto state_for_fn = state;

			return ap(
				FW(self).fn_readerio(std::move(state_for_fn)), FW(self).value_readerio(FW(state)));
		}
	};
};
}  // namespace boost::hana

#include "../../macros_pop.hpp"