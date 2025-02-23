#pragma once
#include <concepts>
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#define FW(a) std::forward<decltype(a)>(a)

namespace vulkandemo::monad
{
// NOLINTBEGIN(*-overloaded-operator,*-trailing-return,*-identifier-length)

namespace detail
{
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
		[func = FW(func), tuple = std::forward_as_tuple(ts...)](auto... Is)
		{ return func(FW(std::get<sizeof...(Ts) - 1>(tuple)), FW(std::get<Is>(tuple))...); });
}

template <typename T, template <typename...> typename U>
concept SpecialisationOf = requires(T * x)
{
	[]<typename... As>(U<As...> *) {}(x);
};

template <class>
constexpr bool always_false = false;

template <class T>
struct FnTraitsImpl
{
	static constexpr bool is_function = false;
};

template <typename R, typename... Args>
struct FnTraitsImpl<std::function<R(Args...)>>
{
	static constexpr bool is_function = true;
	static constexpr std::size_t arity = sizeof...(Args);

	template <std::size_t idx>
	using arg = std::tuple_element_t<idx, std::tuple<Args...>>;

	using return_value = R;
};

template <class Func>
using FnTraits = FnTraitsImpl<decltype(std::function{std::declval<std::decay_t<Func>>()})>;

}  // namespace detail

template <typename F>
concept Transformer = !std::is_void_v<F>;

auto bind(auto &&... ms_and_lifter)
{
	return detail::rotate_right(
		[](auto && lifter, auto && m, auto &&... ms)
		{
			if constexpr (sizeof...(ms) == 0)
			{
				return FW(m).bind(FW(lifter));
			}
			else
			{
				return FW(m).bind(
					[lifter = FW(lifter),
					 ... ms = FW(ms)](auto && arg)	// NOLINT(*-identifier-length)
					{
						return bind(
							FW(ms)...,
							[lifter = FW(lifter), arg = FW(arg), ... ms = FW(ms)](auto &&... args)
							{ return lifter(FW(arg), FW(args)...); });
					});
			}
		},
		FW(ms_and_lifter)...);
}

auto fmapN(auto && transformer, auto && io, auto &&... ios)	 // NOLINT
{
	if constexpr (sizeof...(ios) == 0)
	{
		return FW(io).fmap(FW(transformer));
	}
	else
	{
		return FW(io).fmap(
			[transformer = FW(transformer), ... ios = FW(ios)](auto && arg)
			{
				return fmapN(
					[transformer = FW(transformer), arg = FW(arg), ... ios = FW(ios)](
						auto &&... args) { return transformer(FW(arg), FW(args)...); },
					FW(ios)...);
			});
	}
}

template <typename... Args>
struct Collect
{
	std::tuple<Args...> args;
};

auto collect(auto &&... args)
{
	return Collect{std::tuple{FW(args)...}};
}

decltype(auto) operator>>(detail::SpecialisationOf<Collect> auto && lhs, auto && rhs)
{
	return std::apply(
		[](auto &&... args) { return bind(FW(args)...); },
		std::tuple_cat(FW(lhs).args, std::tuple{FW(rhs)}));
}

namespace io
{

template <typename F>
concept Action = !std::is_void_v<F>;

template <typename F, typename R>
concept Lifter = requires(F func)
{
	func(std::declval<R>());
};

template <Action Act>
struct IO
{
	Act action;
	using A = decltype(action());

	decltype(auto) operator()() const  // NOLINT(*-overloaded-operator)
	{
		return action();
	}

	auto bind(Lifter<A> auto && lifter) const &
	{
		auto next = [prev = action, lifter = FW(lifter)] { return lifter(prev())(); };
		return IO<decltype(next)>{std::move(next)};
	}

	auto bind(Lifter<A> auto && lifter) &&
	{
		auto next = [prev = std::move(action), lifter = FW(lifter)] { return lifter(prev())(); };
		return IO<decltype(next)>{std::move(next)};
	}

	auto fmap(Transformer auto && transformer) const &
	{
		auto next = [prev = action, transformer = FW(transformer)] { return transformer(prev()); };
		return IO<decltype(next)>{std::move(next)};
	}

	auto fmap(Transformer auto && transformer) &&
	{
		auto next = [prev = std::move(action), transformer = FW(transformer)]
		{ return transformer(prev()); };
		return IO<decltype(next)>{std::move(next)};
	}

	auto pair_with(auto && second)
	{
		return fmap([second = FW(second)](auto && value)
					{ return std::pair{FW(value), FW(second)}; });
	}
};

decltype(auto) operator>>(auto && lhs, auto && rhs)
{
	return FW(lhs).bind(FW(rhs));
}
}  // namespace io

namespace stateio
{

struct Empty
{
};

template <typename F>
concept Action = !std::is_void_v<F>;

template <typename F>
concept Lifter = !std::is_void_v<F>;

template <Action Act>
struct StateIO
{
	Act action;

	decltype(auto) operator()(auto && state) const
	{
		return action(FW(state));
	}

	auto bind(Lifter auto && lifter) const &
	{
		auto next = [prev = action, lifter = FW(lifter)](
						auto && state) -> decltype(auto)  // NOLINT(*-trailing-return)
		{
			return prev(FW(state)).bind(
				[lifter = FW(lifter)](auto && value_and_state)
				{ return lifter(FW(value_and_state).first)(FW(value_and_state).second); });
		};
		return StateIO<decltype(next)>(std::move(next));
	}
	auto bind(Lifter auto && lifter) &&
	{
		auto next = [prev = std::move(action), lifter = FW(lifter)](
						auto && state) -> decltype(auto)  // NOLINT(*-trailing-return)
		{
			return prev(FW(state)).bind(
				[lifter = FW(lifter)](auto && value_and_state)
				{ return lifter(FW(value_and_state).first)(FW(value_and_state).second); });
		};
		return StateIO<decltype(next)>(std::move(next));
	}
};

auto lift(detail::SpecialisationOf<io::IO> auto && iom)
{
	return StateIO{[iom = FW(iom)](auto && state)
				   {
					   return FW(iom).bind(
						   [state = FW(state)](auto && value)
						   {
							   return io::IO{[value = FW(value), state = FW(state)]
											 { return std::pair{FW(value), FW(state)}; }};
						   });
				   }};
}

}  // namespace stateio
// NOLINTEND(*-overloaded-operator,*-trailing-return,*-identifier-length)
}  // namespace vulkandemo::monad
