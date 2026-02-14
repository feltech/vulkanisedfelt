#pragma once

#include <boost/hana/fwd/append.hpp>
#include <concepts>
#include <cstddef>
#include <utility>

#include <libfork/core/scheduler.hpp>
#include <libfork/core/sync_wait.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include <immer/detail/rbts/bits.hpp>
#include <immer/vector.hpp>

#include "../macros_push.hpp"

// NOLINTBEGIN(*-overloaded-operator,*-trailing-return,*-identifier-length)

namespace vulkandemo::monad::detail
{

template <typename F>
concept Transformer = !std::is_void_v<F>;

template <class Arg>
struct AsyncFunctorInterface
{
	Arg arg;

	struct async_function_tag_t
	{
	};

	auto sync_wait(this auto && self, lf::scheduler auto && pool)
	{
		return lf::sync_wait(FW(pool), FW(self).fn, FW(self).arg);
	}

	auto sync_wait(this auto && self, std::size_t const pool_size)
	{
		return FW(self).sync_wait(lf::lazy_pool{pool_size});
	}

	auto sync_wait(this auto && self)
	{
		return FW(self).sync_wait(lf::lazy_pool{});
	}
};

template <class T>
struct unwrap_async : std::type_identity<T>
{
	static constexpr bool value = false;
};
template <class T>
requires requires
{
	typename T::async_function_tag_t;
}
struct unwrap_async<T> : std::type_identity<typename T::IOResultType>
{
	static constexpr bool value = true;
};
template <class T>
using unwrap_async_t = unwrap_async<T>::type;
template <class T>
constexpr auto unwrap_async_v = unwrap_async<T>::value;

template <typename Tuple>
struct tuple_appender_t
{
	Tuple vals;
	constexpr auto operator()(this auto && self, auto && value_to_append)
	{
		using boost::hana::append;
		return append(FW(self).vals, FW(value_to_append));
	}
};
}  // namespace vulkandemo::monad::detail

// NOLINTEND(*-overloaded-operator,*-trailing-return,*-identifier-length)

#include "../macros_pop.hpp"
