// IWYU pragma: private, include "../io.hpp"
#pragma once
#include <ranges>
#include <type_traits>

#include <boost/hana/fwd/chain.hpp>
#include <boost/hana/fwd/lift.hpp>
#include <boost/hana/fwd/transform.hpp>

#include "../detail.hpp"
#include "./filter.hpp"
#include "./fwd.hpp"
#include "./traverse.hpp"

#include "../../macros_push.hpp"

namespace vulkandemo::monad::io
{

struct io_tag_t;

constexpr auto pure(auto && value)
{
	return boost::hana::lift<io_tag_t>(FW(value));
}

template <Action Act>
struct IO
{
	Act action;
	using Result = std::invoke_result_t<Act>;
	using Value = detail::unwrap_async_t<Result>;

	decltype(auto) operator()(this auto && self)
	{
		return FW(self).action();
	}

	[[nodiscard]] auto bind(this auto && self, auto && kleisli)
	{
		return boost::hana::chain(FW(self), FW(kleisli));
	}

	[[nodiscard]] auto fmap(this auto && self, auto && transformer)
	{
		return boost::hana::transform(FW(self), FW(transformer));
	}

	[[nodiscard]] auto traverse(this auto && self, auto && kleisli)
	{
		static_assert(std::ranges::range<Value>, "Can only traverse effects that give a range");

		return FW(self).bind(traverse_t::io_t::with_kleisli_t{FW(kleisli)});
	}

	[[nodiscard]] auto filter(this auto && self, auto && kleisli)
	{
		// Note: a good reason to eschew anonymous lambdas is so that we can have ranges of IOs -
		// i.e. where the action type is homogenous, so the IO type as a whole is the same for all
		// elements.
		static_assert(std::ranges::range<Value>, "Can only filter effects that give a range");

		return FW(self).bind(filter_t::io_t::with_kleisli_t{FW(kleisli)});
	}
};
}  // namespace vulkandemo::monad::io

#include "../../macros_pop.hpp"