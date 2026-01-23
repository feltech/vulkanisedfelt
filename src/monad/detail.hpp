#pragma once

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

template <typename F, std::size_t... Is>
constexpr auto indices_impl(F f, [[maybe_unused]] std::index_sequence<Is...> seq)
{
	return f(std::integral_constant<size_t, Is>()...);
}

template <size_t N, typename F>
constexpr auto indices(F f)
{
	return indices_impl(f, std::make_index_sequence<N>());
}

/// Given f and some args t0, t1, ..., tn, calls f(tn, t0, t1, ..., tn-1)
template <typename F, typename... Ts>
constexpr auto rotate_right(F && func, Ts &&... ts)
{
	return indices<sizeof...(Ts) - 1>(
		[func = FW(func), tuple = std::forward_as_tuple(ts...)](auto... is)
		{ return func(FW(std::get<sizeof...(Ts) - 1>(tuple)), FW(std::get<is>(tuple))...); });
}


template <typename T, typename... As>
concept CallableWithResultsOf = requires(T f, As... a)
{
	f(a()...);
};

template <class T>
inline constexpr bool is_tuple_like_v = false;

template <class... Elems>
inline constexpr bool is_tuple_like_v<std::tuple<Elems...>> = true;

template <class T1, class T2>
inline constexpr bool is_tuple_like_v<std::pair<T1, T2>> = true;

template <class T, size_t N>
inline constexpr bool is_tuple_like_v<std::array<T, N>> = true;

template <class It, class Sent, std::ranges::subrange_kind Kind>
inline constexpr bool is_tuple_like_v<std::ranges::subrange<It, Sent, Kind>> = true;

template <class T>
concept tuple_like = is_tuple_like_v<std::remove_cvref_t<T>>;

template <typename T>
concept pair_like = tuple_like<T> && std::tuple_size_v<std::remove_cvref_t<T>> == 2;

template <typename, typename>
struct is_applicable : std::false_type
{
};

template <typename Func, template <typename...> typename Tuple, typename... Args>
struct is_applicable<Func, Tuple<Args...>> : std::is_invocable<Func, Args...>
{
};

template <class F, class T>
concept applicable_with = is_applicable<F, T>::value;

template <typename, typename>
struct apply_result : std::false_type
{
};

template <typename Func, template <typename...> typename Tuple, typename... Args>
struct apply_result<Func, Tuple<Args...>> : std::invoke_result<Func, Args...>
{
};

template <class... Args>
using apply_result_t = apply_result<Args...>;


template <class F, class T>
struct invoke_or_apply_result : apply_result<F, T>
{
};

template <class F, class T>
requires std::invocable<F, T> struct invoke_or_apply_result<F, T> : std::invoke_result<F, T>
{
};

template <class F, class T>
using invoke_or_apply_result_t = invoke_or_apply_result<F, T>::type;
}  // namespace vulkandemo::monad::detail

// NOLINTEND(*-overloaded-operator,*-trailing-return,*-identifier-length)

#include "../macros_pop.hpp"
