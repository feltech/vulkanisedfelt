#pragma once
#include <concepts>
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "macros.hpp"

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

template <typename T, typename... As>
concept CallableWithResultsOf = requires(T f, As... a)
{
	f(a()...);
};

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

template <class T>
concept IsMonad = requires(T t)
{
	{ t.bind([u=t](auto){ return u; }) };
};
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

decltype(auto) operator>>(detail::SpecialisationOf<Collect> auto && lhs, detail::IsMonad auto && rhs)
{
	return Collect{std::tuple_cat(FW(lhs).args, std::tuple{FW(rhs)})};
}

decltype(auto) operator>>(
	detail::SpecialisationOf<Collect> auto && lhs, detail::SpecialisationOf<Collect> auto && rhs)
{
	return Collect{std::tuple_cat(FW(lhs).args, FW(rhs).args)};
}

template<typename F, class C>
concept CallableWithArgsFromCollection = requires (F f, C c)
{
    { std::apply(bind, std::tuple_cat(c.args, std::tuple{f})) };

};

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

template <Action Act>
struct IO;

template <typename F, typename R>
concept Lifter = requires(F func, R io)
{
	{func(io())} -> detail::SpecialisationOf<IO>;
};

template <Action Act>
struct IO
{
	Act action;
	using Ret = decltype(action());

	decltype(auto) operator()() const  // NOLINT(*-overloaded-operator)
	{
		return action();
	}

	auto bind(Lifter<IO> auto && lifter) const &
	{
		auto next = [prev = action, lifter = FW(lifter)] { return lifter(prev())(); };
		return IO<decltype(next)>{std::move(next)};
	}

	auto bind(Lifter<IO> auto && lifter) &&
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

	template <typename... As>
	requires detail::CallableWithResultsOf<Ret, As...> auto apply(IO<As> &&... ios) const &
	{
		auto next = [prev = action, ... ios = FW(ios)] { return prev()(ios()...); };
		return IO<decltype(next)>{std::move(next)};
	}

	template <typename... As>
	requires detail::CallableWithResultsOf<Ret, As...> auto apply(IO<As> &&... ios) &&
	{
		auto next = [prev = std::move(action), ... ios = FW(ios)] { return prev()(ios()...); };
		return IO<decltype(next)>{std::move(next)};
	}

	auto pair_with(auto && second)
	{
		return fmap([second = FW(second)](auto && value)
					{ return std::pair{FW(value), FW(second)}; });
	}
};

decltype(auto) operator>>(detail::SpecialisationOf<IO> auto && lhs, auto && rhs)
{
	return FW(lhs).bind(FW(rhs));
}
decltype(auto) operator>>(
	detail::SpecialisationOf<IO> auto && lhs, detail::SpecialisationOf<Collect> auto && rhs)
{
	return Collect{std::tuple_cat(std::tuple{FW(lhs)}, FW(rhs).args)};
}
decltype(auto) operator>>(
	detail::SpecialisationOf<IO> auto && lhs, detail::SpecialisationOf<IO> auto && rhs)
{
	return collect(lhs, rhs);
}
}  // namespace io

namespace stateio
{

struct Empty
{
};

template <typename F>
concept Action = !std::is_void_v<F>;

template <Action Act>
struct StateIO;

template <typename F, typename I>
concept Lifter = requires(F f, I io)
{
	requires detail::SpecialisationOf<I, StateIO>;
    // TODO(DF): Figure out how to constrain a lifter, given that we cannot know ahead of time what
    //  the state type will be.

};

template <Action Act>
struct StateIO
{
	Act action;
	// using Ret = typename detail::FnTraits<Act>::return_value;

	decltype(auto) operator()(auto && state) const
	{
		return action(FW(state));
	}

	auto bind(Lifter<StateIO> auto && lifter) const &
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
	auto bind(Lifter<StateIO> auto && lifter) &&
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
