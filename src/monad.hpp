#pragma once
#include <algorithm>
#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/fwd/ap.hpp>
#include <boost/hana/fwd/append.hpp>
#include <boost/hana/fwd/chain.hpp>
#include <boost/hana/fwd/fold_left.hpp>
#include <boost/hana/fwd/lift.hpp>
#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/hana.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/fwd/core/to.hpp>
#include <boost/hana/fwd/transform.hpp>
#include <boost/hana/fwd/tuple.hpp>

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

constexpr decltype(auto) ensure_tuple(auto && value)
{
	if constexpr (SpecialisationOf<std::decay_t<decltype(value)>, std::tuple>)
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
// using monad::fmap;	// For ADL?.

struct io_tag_t
{
};

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
}
|| requires(F func, R io)
{
	{
		std::apply(func, io())
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
}  // namespace io
}  // namespace vulkandemo::monad
namespace boost::hana
{
namespace io = vulkandemo::monad::io;
using vulkandemo::monad::detail::ensure_tuple;

template <typename A>
struct tag_of<io::IO<A>>
{
	using type = io::io_tag_t;
};

template <>
struct lift_impl<io::io_tag_t>
{
	static auto apply(auto && value)
	{
		return io::IO{io_action_t{FW(value)}};
	}

	template <class Value>
	struct io_action_t
	{
		Value value;
		constexpr auto operator()() const
		{
			return value;
		}
	};
};

template <>
struct chain_impl<io::io_tag_t>
{
	static auto apply(auto && iom, auto && lifter)
	{
		return io::IO{io_action_t{FW(iom), FW(lifter)}};
	}

	template <class WrappedIO, class Lifter>
	struct io_action_t
	{
		WrappedIO iom;
		Lifter lifter;
		constexpr auto operator()() const
		{
			if constexpr (requires { lifter(iom()); })
			{
				auto value = iom();
				auto new_io = lifter(std::move(value));
				auto new_value = std::move(new_io)();
				return new_value;
			}
			else if constexpr (requires { std::apply(lifter, iom()); })
			{
				return std::apply(lifter, iom())();
			}
			else
			{
				static_assert(false, "IO lifter is not callable with value");
			}
		}
	};
};

template <>
struct transform_impl<io::io_tag_t>
{
	static auto apply(auto && iom, auto && transformer)
	{
		return io::IO{io_action_t{FW(iom), FW(transformer)}};
	}

	template <class WrappedIO, class Transformer>
	struct io_action_t
	{
		WrappedIO iom;
		Transformer transformer;
		constexpr auto operator()() const
		{
			if constexpr (requires { transformer(iom()); })
			{
				return transformer(iom());
			}
			else
			{
				static_assert(
					requires { std::apply(transformer, iom()); },
					"IO value transformer is not callable with value");

				return std::apply(transformer, iom());
			}
		}
	};
};

template <>
struct ap_impl<io::io_tag_t>
{
	static auto apply(auto && fn_io, auto && value_io)
	{
		return io::IO{io_action_t{FW(fn_io), FW(value_io)}};
	}

	template <class FnIO, class ValueIO>
	struct io_action_t
	{
		FnIO fn_io;
		ValueIO value_io;
		constexpr auto operator()() const
		{
			// TODO(DF): fn and value can be computed concurrently.
			auto fn = fn_io();
			auto value = value_io();

			auto result = fn(value);
			return result;
		}
	};
};
}  // namespace boost::hana

namespace vulkandemo::monad
{
namespace io
{

constexpr auto lift(auto && value)
{
	return boost::hana::lift<io_tag_t>(FW(value));
}

template <Action Act>
struct IO
{
	Act action;
	using Ret = decltype(action());

	decltype(auto) operator()() const  // NOLINT(*-overloaded-operator)
	{
		return action();
	}

	[[nodiscard]] auto bind(this auto && self, Lifter<IO> auto && lifter)
	{
		return boost::hana::chain(FW(self), FW(lifter));
	}

	[[nodiscard]] auto fmap(this auto && self, Transformer auto && transformer)
	{
		return boost::hana::transform(FW(self), FW(transformer));
	}

	template <typename OtherAction>
	[[nodiscard]] auto zip(IO<OtherAction> rhs_io)
	{
		return bind(
			[rhs_io = std::move(rhs_io)](Ret lhs_value)
			{
				return rhs_io.fmap(
					[lhs_value = std::move(lhs_value)](typename IO<OtherAction>::Ret rhs_value)
					{
						return std::tuple_cat(
							detail::ensure_tuple(lhs_value),
							detail::ensure_tuple(std::move(rhs_value)));
					});
			});
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

auto sequence(detail::SpecialisationOf<IO> auto &&... ms)
{
	using boost::hana::ap;
	using boost::hana::append;
	using boost::hana::fold_left;
	using boost::hana::lift;

	return fold_left(
		std::tuple{FW(ms)...},
		lift<io_tag_t>(std::tuple{}),
		[](auto && acc, auto && iom)
		{
			// Applicative - unwrap two IOs, the first yielding a function and the second yielding a
			// value, then call the function with the value in a new IO. In this case, append the
			// result of an io to a tuple.
			return ap(
				// Transform IO result from a tuple of values to a function that appends a value
				// to the (captured) tuple and returns the new tuple.
				acc.fmap(
					[](auto && values)
					{
						return [values = FW(values)](auto && value_to_append)
						{ return append(values, FW(value_to_append)); };
					}),
				FW(iom));
			;
		});
}

auto fmap(auto &&... ms_and_transformer)
{
	return detail::rotate_right(
		[](auto && transformer, detail::SpecialisationOf<IO> auto &&... ms)
		{ return sequence(FW(ms)...).fmap(FW(transformer)); },
		FW(ms_and_transformer)...);
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
struct stateio_tag_t
{
};

template <typename Act>
struct StateIO
{
	Act action;
	// using Ret = typename detail::FnTraits<Act>::return_value;

	decltype(auto) operator()(auto && state) const
	{
		// Must return an IO monad that itself returns a pair.
		static_assert(
			detail::SpecialisationOf<decltype(action(state)), io::IO> &&
				detail::SpecialisationOf<decltype(action(state)()), std::pair>,
			"StateIO action must return IO<pair<value, state>>");
		return action(FW(state));
	}

	auto bind(this auto && self, auto && lifter)
	{
		return boost::hana::chain(FW(self), FW(lifter));
	}

	auto fmap(this auto && self, auto && transformer)
	{
		return boost::hana::transform(FW(self), FW(transformer));
	}

	auto then(detail::SpecialisationOf<StateIO> auto && stateiom) const
	{
		return this->bind([stateiom = FW(stateiom)]([[maybe_unused]] auto &&... unused)
						  { return stateiom; });
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
		// static_assert(
		// 	requires(A action, S state, L lifter) { {lifter(action(state)().first)}; },
		// 	"StateIO lifter function has incorrect arguments");
		// static_assert(
		// 	requires(A action, S state, L lifter) {
		// 		{
		// 			lifter(action(state)().first)
		// 		} -> detail::SpecialisationOf<StateIO>;
		// 	},
		// 	"StateIO lifter function must return a StateIO");

		return true;
	}
};
}  // namespace stateio
}  // namespace vulkandemo::monad

namespace boost::hana
{
namespace io = vulkandemo::monad::io;
namespace stateio = vulkandemo::monad::stateio;
using vulkandemo::monad::detail::SpecialisationOf;

template <typename A>
struct tag_of<stateio::StateIO<A>>
{
	using type = stateio::stateio_tag_t;
};

template <>
struct lift_impl<stateio::stateio_tag_t>
{
	// TODO(DF): Perhaps can be removed, depending how clever hana::lift<io_tag_t> is.
	static auto apply(SpecialisationOf<io::IO> auto && iom)
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
			return iom.pair_with(FW(state));
		}
	};
};

template <>
struct chain_impl<stateio::stateio_tag_t>
{
	constexpr static auto apply(auto && stateiom, auto && lifter)
	{
		return stateio::StateIO{stateio_action_t{FW(stateiom), FW(lifter)}};
	}

	template <class WrappedStateIO, class StateIOLifter>
	struct stateio_action_t
	{
		WrappedStateIO stateiom;
		StateIOLifter lifter;
		constexpr auto operator()(auto && state) const
		{
			auto iom = stateiom(FW(state));
			return std::move(iom).bind(io_lifter_t{lifter});
		}
	};

	template <class StateIOLifter>
	struct io_lifter_t
	{
		StateIOLifter lifter;
		constexpr auto operator()(auto value_and_state) const
		{
			static_assert(
				requires { lifter(value_and_state.first); } ||
					requires { std::apply(lifter, value_and_state.first); },
				"StateIO lifter is not callable with value");

			if constexpr (requires { std::apply(lifter, value_and_state.first); })
			{
				auto new_stateio = std::apply(lifter, std::move(value_and_state.first));
				auto new_io = std::move(new_stateio)(std::move(value_and_state.second));
				return new_io;
			}
			else
			{
				auto new_stateio = lifter(std::move(value_and_state.first));
				auto new_io = std::move(new_stateio)(std::move(value_and_state.second));
				return new_io;
			}
		}
	};
};

template <>
struct transform_impl<stateio::stateio_tag_t>
{
	static auto apply(auto && stateiom, auto && transformer)
	{
		return stateio::StateIO{stateio_action_t{FW(stateiom), FW(transformer)}};
	}

	template <class WrappedStateIO, class Transformer>
	struct stateio_action_t
	{
		WrappedStateIO stateiom;
		Transformer transformer;

		constexpr auto operator()(auto const state) const
		{
			return stateiom(state).fmap(io_transformer_t{transformer, state});
		}
	};

	template <class State, class ValueTransformer>
	struct io_transformer_t
	{
		ValueTransformer transformer;
		State state;

		constexpr auto operator()(auto && value_and_state) const
		{
			if constexpr (requires { transformer(value_and_state.first); })
			{
				return std::pair{
					transformer(FW(value_and_state).first), FW(value_and_state).second};
			}
			else if constexpr (requires { std::apply(transformer, value_and_state.first); })
			{
				return std::pair{
					std::apply(transformer, FW(value_and_state).first), FW(value_and_state).second};
			}
			else
			{
				static_assert(false, "StateIO value transformer is not callable with value");
			}
		}
	};
};

template <>
struct ap_impl<stateio::stateio_tag_t>
{
	// Note that StateIO is inherently sequential since state must be threaded
	// through, so may as well use nested bind() calls (unlike e.g. ap<io_tag_t>).
	static auto apply(auto && lhs, auto && rhs)
	{
		return FW(lhs).bind(fn_lifter_t{FW(rhs)});
	}

	template <class ValueStateIO>
	struct fn_lifter_t
	{
		ValueStateIO value_stateio;

		constexpr auto operator()(auto && fn) const
		{
			return value_stateio.bind(value_lifter_t{FW(fn)});
		}
	};

	template <class Fn>
	struct value_lifter_t
	{
		Fn fn;
		constexpr auto operator()(auto && value) const
		{
			return lift<stateio::stateio_tag_t>(fn(FW(value)));
		}
	};
};
}  // namespace boost::hana

namespace vulkandemo::monad
{
namespace stateio
{

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
		constexpr auto operator()() const
		{
			return std::pair{state, state};
		}
	};

	struct stateio_action_t
	{
		constexpr auto operator()(auto state) const
		{
			return io::IO{io_action_t{std::move(state)}};
		}
	};

	static constexpr auto make_stateio()
	{
		return StateIO{stateio_action_t{}};
	}
	constexpr auto operator()([[maybe_unused]] auto &&... unused) const
	{
		return make_stateio();
	}
};

constexpr auto lift(auto && value)
{
	return boost::hana::lift<stateio_tag_t>(FW(value));
}

auto sequence(detail::SpecialisationOf<StateIO> auto &&... ms)
{
	using boost::hana::ap;
	using boost::hana::append;
	using boost::hana::fold_left;

	return fold_left(
		std::tuple{FW(ms)...},
		lift(std::tuple{}),
		[](auto && acc, auto && iom)
		{
			// Applicative - unwrap two IOs, the first yielding a function and the second
			// yielding a value, then call the function with the value in a new IO. In this
			// case, append the result of an io to a tuple.
			return ap(
				// Transform IO result from a tuple of values to a function that appends a value
				// to the (captured) tuple and returns the new tuple.
				acc.fmap(
					[](auto && values)
					{
						return [values = FW(values)](auto && value_to_append)
						{ return append(values, FW(value_to_append)); };
					}),
				FW(iom));
			;
		});
}
}  // namespace stateio

// NOLINTEND(*-overloaded-operator,*-trailing-return,*-identifier-length)
}  // namespace vulkandemo::monad
