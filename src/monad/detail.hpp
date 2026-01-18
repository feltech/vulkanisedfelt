#pragma once

#include <concepts>
#include <immer/detail/rbts/bits.hpp>
#include <immer/vector.hpp>
#include <libfork/core/sync_wait.hpp>
#include <libfork/schedule/lazy_pool.hpp>
#include <utility>

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

	struct async_function_tag
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
	typename T::async_function_tag;
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

template <template <typename...> class Template, typename... Args>
void is_specialisation_of(Template<Args...> /*unused*/)
{
}

template <class T, template <typename...> class Template>
concept specialisation_of = requires(T t)
{
	is_specialisation_of<Template>(t);
};

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

constexpr decltype(auto) ensure_tuple(auto && value)
{
	if constexpr (specialisation_of<std::decay_t<decltype(value)>, std::tuple>)
	{
		return FW(value);
	}
	else
	{
		return std::tuple{FW(value)};
	}
}

template <class>
struct FnTraitsImpl
{
	static constexpr bool kIsFunction = false;
};

template <typename R, typename... Args>
struct FnTraitsImpl<std::function<R(Args...)>>
{
	static constexpr bool kIsFunction = true;
	static constexpr std::size_t kArity = sizeof...(Args);

	template <std::size_t idx>
	using Arg = std::tuple_element_t<idx, std::tuple<Args...>>;

	using ReturnValue = R;
};

template <class Func>
using FnTraits = FnTraitsImpl<decltype(std::function{std::declval<std::decay_t<Func>>()})>;

template <class T>
concept IsMonad = requires(T t)
{
	{t.fmap([](decltype(t())) { return 0; })};
};

template <class T, typename R>
concept IOTo = requires(T io)
{
	{io()}->std::convertible_to<R>;
};

template <typename F, typename A>
concept MappingFrom = requires(F func, A value)
{
	{func(value)};
};

template <typename Container>
struct Unspecialise;

template <template <typename...> class Container, typename... OldArgs>
struct Unspecialise<Container<OldArgs...>>
{
	template <typename... Args>
	using Specialise = Container<Args...>;
};

template <
	typename T,
	typename Policy,
	immer::detail::rbts::bits_t B,
	immer::detail::rbts::bits_t BL>
struct Unspecialise<immer::vector<T, Policy, B, BL>>
{
	template <typename NewT>
	using Specialise = immer::vector<NewT, Policy, B, BL>;
};

template <typename Container, typename NewType>
using Respecialise = Unspecialise<Container>::template Specialise<NewType>;

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
