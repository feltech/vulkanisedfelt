#pragma once
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <ranges>
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
		[func = FW(func), tuple = std::forward_as_tuple(ts...)](auto... is)
		{ return func(FW(std::get<sizeof...(Ts) - 1>(tuple)), FW(std::get<is>(tuple))...); });
}

template <template <typename...> class Template, typename... Args>
void is_specialisation_of(Template<Args...> const & /*unused*/)
{
}

template <class T, template <typename...> class Template>
concept SpecialisationOf = requires(T t)
{
	is_specialisation_of<Template>(t);
};

template <typename T, typename... As>
concept CallableWithResultsOf = requires(T f, As... a)
{
	f(a()...);
};

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
	{
		io()
	} -> std::convertible_to<R>;
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

template <typename Container, typename NewType>
using Respecialise = typename Unspecialise<Container>::template Specialise<NewType>;
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
							ms...,
							[lifter, arg = FW(arg)](auto &&... args)
							{ return lifter(arg, FW(args)...); });
					});
			}
		},
		FW(ms_and_lifter)...);
}

auto fmap(auto &&... ms_and_transformer)
{
	return detail::rotate_right(
		[](auto && transformer, auto && m, auto &&... ms)
		{
			if constexpr (sizeof...(ms) == 0)
			{
				return FW(m).fmap(FW(transformer));
			}
			else
			{
				return FW(m).bind(
					[transformer = FW(transformer),
					 ... ms = FW(ms)](auto && arg)	// NOLINT(*-identifier-length)
					{
						return fmap(
							ms...,
							[transformer, arg = FW(arg)](auto &&... args)
							{ return transformer(arg, FW(args)...); });
					});
			}
		},
		FW(ms_and_transformer)...);
}

namespace detail
{
template <typename... Args>
struct Collect
{
	std::tuple<Args...> args;
};

decltype(auto) operator>>(SpecialisationOf<Collect> auto && lhs, IsMonad auto && rhs)
{
	return Collect{std::tuple_cat(FW(lhs).args, std::tuple{FW(rhs)})};
}

decltype(auto) operator>>(
	SpecialisationOf<Collect> auto && lhs, SpecialisationOf<Collect> auto && rhs)
{
	return Collect{std::tuple_cat(FW(lhs).args, FW(rhs).args)};
}

template <typename F, class C>
concept CallableWithArgsFromCollection = requires(F f, C c)
{
	{std::apply(bind, std::tuple_cat(c.args, std::tuple{f}))};
};

decltype(auto) operator>>(SpecialisationOf<Collect> auto && lhs, auto && rhs)
{
	return std::apply(
		[](auto &&... args) { return bind(FW(args)...); },
		std::tuple_cat(FW(lhs).args, std::tuple{FW(rhs)}));
}
}  // namespace detail

namespace io
{
using monad::bind;	// For ADL?.
using monad::fmap;	// For ADL?.

template <typename F>
concept Action = !std::is_void_v<F>;

template <Action Act>
struct IO;

template <class T, class R>
concept IOFor = requires(T t)
{
	{
		t()
	} -> std::convertible_to<R>;
};

template <typename F, typename R>
concept Lifter = requires(F func, R io)
{
	{
		func(io())
	} -> detail::SpecialisationOf<IO>;
};

template <typename F, typename I, typename R>
concept LifterTo = requires(F func, I io)
{
	{
		func(io())
	} -> IOFor<R>;
};

template <typename F, typename E, typename R>
concept LifterFromTo = requires(F func, E elem)
{
	{
		func(elem)
	} -> IOFor<R>;
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

	[[nodiscard]] auto bind(Lifter<IO> auto && lifter) const &
	{
		auto next = [prev = action, lifter = FW(lifter)] { return lifter(prev())(); };
		return IO<decltype(next)>{std::move(next)};
	}

	[[nodiscard]] auto bind(Lifter<IO> auto && lifter) &&
	{
		auto next = [prev = std::move(action), lifter = FW(lifter)] { return lifter(prev())(); };
		return IO<decltype(next)>{std::move(next)};
	}

	[[nodiscard]] auto fmap(Transformer auto && transformer) const &
	{
		auto next = [prev = action, transformer = FW(transformer)] { return transformer(prev()); };
		return IO<decltype(next)>{std::move(next)};
	}

	[[nodiscard]] auto fmap(Transformer auto && transformer) &&
	{
		auto next = [prev = std::move(action), transformer = FW(transformer)]
		{ return transformer(prev()); };
		return IO<decltype(next)>{std::move(next)};
	}

	template <typename... As>
	requires detail::CallableWithResultsOf<Ret, As...> [[nodiscard]] auto apply(
		IO<As> &&... ios) const &
	{
		auto next = [prev = action, ... ios = FW(ios)] { return prev()(ios()...); };
		return IO<decltype(next)>{std::move(next)};
	}

	template <typename... As>
	requires detail::CallableWithResultsOf<Ret, As...> [[nodiscard]] auto apply(IO<As> &&... ios) &&
	{
		auto next = [prev = std::move(action), ... ios = FW(ios)] { return prev()(ios()...); };
		return IO<decltype(next)>{std::move(next)};
	}

	[[nodiscard]] auto traverse(auto && element_lifter) const requires std::ranges::range<Ret>
	{
		using InputContainer = Ret;

		return bind(
			[element_lifter = FW(element_lifter)](AUTO(InputContainer) range)
			{
				auto next = [element_lifter, range = FW(range)]
				{
					return range |
						std::views::transform([element_lifter](auto && elem)
											  { return element_lifter(elem)(); }) |
						ranges::to<detail::Unspecialise<InputContainer>::template Specialise>;
				};
				return IO<decltype(next)>{std::move(next)};
			});
	}

	[[nodiscard]] auto filter(LifterFromTo<typename Ret::value_type, bool> auto && element_lifter)
		const requires std::ranges::range<Ret>
	{
		// Note: due to IO being templated on a lambda, mapping elements to IO means a different
		// type for each element. This means we cannot have a container of IOs. It also means
		// recursive algorithms can hit the max template depth.

		return bind(
			[element_lifter = FW(element_lifter)](AUTO(Ret) range)
			{
				auto next = [element_lifter, range = FW(range)]
				{
					auto new_range = range;
					std::ranges::remove_if(
						new_range, [](auto && io) { return FW(io)(); }, element_lifter);
					return new_range;
				};
				return IO<decltype(next)>{std::move(next)};
			});
	}

	// [[nodiscard]] auto compact() const requires std::ranges::range<Ret>
	// {
	// 	return filter([](typename Ret::value_type const & elem)
	// 				  { return static_cast<bool>(elem); });
	// }

	[[nodiscard]] auto compact() const requires
		std::ranges::range<Ret> && detail::SpecialisationOf<typename Ret::value_type, std::optional>
	{
		using InputContainer = Ret;
		return fmap(
			[](auto && range)
			{
				return range |
					std::views::filter([](auto && elem) { return FW(elem).has_value(); }) |
					std::views::transform([](auto && elem) { return *FW(elem); }) |
					ranges::to<detail::Unspecialise<InputContainer>::template Specialise>;
			});
	}

	[[nodiscard]] auto pair_with(auto && second) const
	{
		return fmap([second = FW(second)](auto && value) { return std::pair{FW(value), second}; });
	}

	[[nodiscard]] auto as_bool() const requires std::ranges::range<Ret>
	{
		return fmap([](auto && range) { return !std::ranges::empty(FW(range)); });
	}
};

auto zip(detail::SpecialisationOf<IO> auto &&... ms)
{
	return fmap(FW(ms)..., [](auto &&... args) { return std::tuple{args...}; });
}

decltype(auto) operator>>(detail::SpecialisationOf<IO> auto && lhs, auto && rhs)
{
	return FW(lhs).bind(FW(rhs));
}
decltype(auto) operator>>(
	detail::SpecialisationOf<IO> auto && lhs, detail::SpecialisationOf<detail::Collect> auto && rhs)
{
	return detail::Collect{std::tuple_cat(std::tuple{FW(lhs)}, FW(rhs).args)};
}
decltype(auto) operator>>(
	detail::SpecialisationOf<IO> auto && lhs, detail::SpecialisationOf<IO> auto && rhs)
{
	return detail::Collect(std::tuple{FW(lhs), FW(rhs)});
}
}  // namespace io

namespace stateio
{
template <typename Act>
struct StateIO;

template <typename Act>
struct StateIO
{
	Act action;
	// using Ret = typename detail::FnTraits<Act>::return_value;

	decltype(auto) operator()(auto && state) const
		// Must return an IO monad that itself returns a pair.
		requires detail::SpecialisationOf<decltype(action(state)), io::IO> && detail::
			SpecialisationOf<decltype(action(state)()), std::pair>
	{
		return action(FW(state));
	}

	auto bind(auto && lifter) const &
	{
		auto next = [prev = action, lifter = FW(lifter)](
						auto && state) -> decltype(auto)  // NOLINT(*-trailing-return)
		{
			static_assert(assert_valid_bind<decltype(prev), decltype(lifter), decltype(state)>());

			return prev(FW(state)).bind(
				[lifter](auto && value_and_state)
				{ return lifter(FW(value_and_state).first)(FW(value_and_state).second); });
		};
		return StateIO<decltype(next)>(std::move(next));
	}

	auto bind(auto && lifter) &&
	{
		auto next = [prev = std::move(action), lifter = FW(lifter)](
						auto && state) -> decltype(auto)  // NOLINT(*-trailing-return)
		{
			static_assert(assert_valid_bind<decltype(prev), decltype(lifter), decltype(state)>());

			return prev(FW(state)).bind(
				[lifter](auto value_and_state)
				{
					return lifter(std::move(value_and_state.first))(
						std::move(value_and_state.second));
				});
		};
		return StateIO<decltype(next)>(std::move(next));
	}

	static auto lift(detail::SpecialisationOf<io::IO> auto && iom)
	{
		return StateIO{[iom = FW(iom)](auto && state) { return iom.pair_with(FW(state)); }};
	}

	auto with_state(auto && io_from_state) const &
	{
		auto next = [prev = action, io_from_state = FW(io_from_state)](
						auto && state) -> decltype(auto)  // NOLINT(*-trailing-return)
		{
			return prev(FW(state)).bind(
				[io_from_state](auto && value_and_state)
				{
					auto [value, new_state] = value_and_state;

					return io_from_state(new_state).fmap(
						[new_state](auto && new_value)
						{ return std::pair{FW(new_value), new_state}; });
				});
		};
		return StateIO<decltype(next)>(std::move(next));
	}

	auto with_state(auto && io_from_state) &&
	{
		auto next = [prev = std::move(action), io_from_state = FW(io_from_state)](
						auto && prev_state) -> decltype(auto)  // NOLINT(*-trailing-return)
		{
			return prev(FW(prev_state))
				.bind(
					[io_from_state](auto && value_and_state)
					{
						auto [value, new_state] = value_and_state;

						return io_from_state(new_state).fmap(
							[new_state](auto && new_value)
							{ return std::pair{FW(new_value), new_state}; });
					});
		};
		return StateIO<decltype(next)>(std::move(next));
	}

	template <typename A, typename L, typename S>
	static constexpr bool assert_valid_bind()
	{
		static_assert(
			requires(A action, S state) {
				{
					action(state)
				} -> detail::SpecialisationOf<io::IO>;
			},
			"StateIO action must return an IO");
		static_assert(
			requires(A action, S state) {
				{
					action(state)()
				} -> detail::SpecialisationOf<std::pair>;
			},
			"StateIO's IO action must return a pair");
		static_assert(
			requires(A action, S state, L lifter) { {lifter(action(state)().first)}; },
			"StateIO lifter function has incorrect arguments");
		static_assert(
			requires(A action, S state, L lifter) {
				{
					lifter(action(state)().first)
				} -> detail::SpecialisationOf<StateIO>;
			},
			"StateIO lifter function must return a StateIO");

		return true;
	}
};

auto lift(detail::SpecialisationOf<io::IO> auto && iom)
{
	return StateIO{[iom = FW(iom)](auto && state) { return iom.pair_with(FW(state)); }};
}

auto with_state(auto && io_cont_from_state)
{
	return StateIO{[io_cont_from_state = FW(io_cont_from_state)](auto && state)
				   { return io_cont_from_state(state).pair_with(FW(state)); }};
}

struct get_t
{
	template <class State>
	struct io_action_t
	{
		State state;
		constexpr auto operator()()const
		{
			return std::pair{state, state};
		}
	};

	struct stateio_action_t
	{
		constexpr auto operator()(auto state)
		{
			return io::IO{io_action_t{std::move(state)}};
		}
	};

	static constexpr auto make_stateio()
	{
		return StateIO{stateio_action_t{}};
	}

	constexpr auto operator()([[maybe_unused]] auto const&... unused) const
	{
		return make_stateio();
	}
};

}  // namespace stateio

// NOLINTEND(*-overloaded-operator,*-trailing-return,*-identifier-length)
}  // namespace vulkandemo::monad
