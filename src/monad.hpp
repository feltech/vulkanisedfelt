#pragma once
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/fwd/ap.hpp>
#include <boost/hana/fwd/append.hpp>
#include <boost/hana/fwd/chain.hpp>
#include <boost/hana/fwd/fold_left.hpp>
#include <boost/hana/fwd/lift.hpp>

#include <libfork/algorithm/lift.hpp>
#include <libfork/core.hpp>
#include <libfork/core/control_flow.hpp>
#include <libfork/core/eventually.hpp>
#include <libfork/core/just.hpp>
#include <libfork/core/scheduler.hpp>
#include <libfork/core/task.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include <boost/hana.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/fwd/core/to.hpp>
#include <boost/hana/fwd/transform.hpp>

#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include "hof.hpp"
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

decltype(auto) operator>>(specialisation_of<Collect> auto && lhs, IsMonad auto && rhs)
{
	return Collect{std::tuple_cat(FW(lhs).args, std::tuple{FW(rhs)})};
}

decltype(auto) operator>>(
	specialisation_of<Collect> auto && lhs, specialisation_of<Collect> auto && rhs)
{
	return Collect{std::tuple_cat(FW(lhs).args, FW(rhs).args)};
}

template <typename F, class C>
concept CallableWithArgsFromCollection = requires(F f, C c)
{
	{std::apply(bind, std::tuple_cat(c.args, std::tuple{f}))};
};

decltype(auto) operator>>(specialisation_of<Collect> auto && lhs, auto && rhs)
{
	return std::apply(
		[](auto &&... args) { return bind(FW(args)...); },
		std::tuple_cat(FW(lhs).args, std::tuple{FW(rhs)}));
}

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
	{t()}->std::convertible_to<R>;
};

template <typename F, typename I>
concept Lifter = true;
//=
// detail::tuple_like<wrapped_io_value_t<I>> && detail::applicable_with<F, wrapped_io_value_t<I>> &&
// 	detail::specialisation_of<detail::apply_result_t<F, wrapped_io_value_t<I>>, IO> ||
// std::invocable<F, wrapped_io_value_t<I>> &&
// 	detail::specialisation_of<std::invoke_result_t<F, wrapped_io_value_t<I>>, IO>;

template <typename F, typename I, typename R>
concept LifterTo = requires(F func, I io)
{
	{func(io())}->IOFor<R>;
};

template <typename F, typename E, typename R>
concept LifterFromTo = true;
// 	requires(F func, E elem)
// {
// 	{
// 		func(elem)
// 	} -> IOFor<R>;
// };
}  // namespace io
}  // namespace vulkandemo::monad
namespace boost::hana
{
namespace io = vulkandemo::monad::io;
using vulkandemo::monad::detail::applicable_with;
using vulkandemo::monad::detail::apply_result;
using vulkandemo::monad::detail::apply_result_t;
using vulkandemo::monad::detail::AsyncFunctorInterface;
using vulkandemo::monad::detail::invoke_or_apply_result_t;
using vulkandemo::monad::detail::specialisation_of;
using vulkandemo::monad::detail::unwrap_async_t;
using vulkandemo::monad::detail::unwrap_async_v;

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
		constexpr auto operator()(this auto && self)
		{
			return FW(self).value;
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

		constexpr auto operator()(this auto && self)
		{
			return async_function_t{std::tuple{FW(self).iom, FW(self).lifter}};
		}
	};

	template <class Arg>
	struct async_function_t : AsyncFunctorInterface<Arg>
	{
		using ArgsTuple = Arg;
		using InputIO = std::tuple_element_t<0, ArgsTuple>;
		using Kleisli = std::tuple_element_t<1, ArgsTuple>;

		using InputIOResult = std::invoke_result_t<InputIO>;
		using InputIOValue = unwrap_async_t<InputIOResult>;
		static constexpr bool kIsInputIOAsync = unwrap_async_v<InputIOResult>;

		using KleisliIO = invoke_or_apply_result_t<Kleisli, InputIOValue>;
		using KleisliIOResult = std::invoke_result_t<KleisliIO>;
		using KleisliIOValue = unwrap_async_t<KleisliIOResult>;
		static constexpr bool kIsKleisliIOAsync = unwrap_async_v<KleisliIOResult>;

		using IOResultType = KleisliIOValue;

		static constexpr auto fn = []([[maybe_unused]] auto fn,
									  ArgsTuple args_tuple) -> lf::task<IOResultType>
		{
			auto const & [input_io, kleisli_fn] = args_tuple;
			lf::eventually<InputIOValue> input_value;

			if constexpr (kIsInputIOAsync)
			{
				auto input_async_fn = input_io();

				input_value = co_await lf::just[input_async_fn.fn](std::move(input_async_fn.arg));
			}
			else
			{
				input_value = input_io();
			}

			auto new_io = [&]
			{
				if constexpr (std::invocable<Kleisli, InputIOValue>)
				{
					return kleisli_fn(std::move(*input_value));
				}
				else if constexpr (applicable_with<Kleisli, InputIOValue>)
				{
					return std::apply(kleisli_fn, std::move(*input_value));
				}
				else
				{
					static_assert(false, "IO lifter is not callable with value");
				}
			}();

			lf::eventually<KleisliIOValue> new_value;

			if constexpr (kIsKleisliIOAsync)
			{
				auto new_async_fn = new_io();
				new_value = co_await lf::just[new_async_fn.fn](std::move(new_async_fn.arg));
			}
			else
			{
				new_value = new_io();
			}

			co_return * new_value;
		};
	};
	template <class Arg>
	async_function_t(Arg) -> async_function_t<Arg>;
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
		constexpr auto operator()(this auto && self)
		{
			return async_function_t{std::tuple{FW(self).iom, FW(self).transformer}};
		}
	};

	template <class Arg>
	struct async_function_t : AsyncFunctorInterface<Arg>
	{
		using ArgsTuple = Arg;

		using InputIO = std::tuple_element_t<0, ArgsTuple>;
		using Transform = std::tuple_element_t<1, ArgsTuple>;

		using InputIOResult = std::invoke_result_t<InputIO>;
		using InputIOValue = unwrap_async_t<InputIOResult>;
		static constexpr bool kIsInputIOAsync = unwrap_async_v<InputIOResult>;

		using TransformIOValue = invoke_or_apply_result_t<Transform, InputIOValue>;

		using IOResultType = TransformIOValue;

		static constexpr auto fn = []([[maybe_unused]] auto fn,
									  ArgsTuple args_tuple) -> lf::task<IOResultType>
		{
			auto const & [input_io, transform_fn] = args_tuple;
			lf::eventually<InputIOValue> input_value;

			if constexpr (kIsInputIOAsync)
			{
				auto input_async_fn = input_io();

				input_value = co_await lf::just[input_async_fn.fn](std::move(input_async_fn.arg));
			}
			else
			{
				input_value = input_io();
			}

			if constexpr (std::invocable<Transform, InputIOValue>)
			{
				co_return transform_fn(*std::move(input_value));
			}
			else if constexpr (applicable_with<Transform, InputIOValue>)
			{
				co_return std::apply(transform_fn, *std::move(input_value));
			}
			else
			{
				static_assert(false, "IO value transformer is not callable with value");
			}
		};
	};
	template <class Arg>
	async_function_t(Arg) -> async_function_t<Arg>;
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
		constexpr auto operator()(this auto && self)
		{
			return async_function_t{std::tuple{FW(self).fn_io, FW(self).value_io}};
		}
	};

	template <class Arg>
	struct async_function_t : AsyncFunctorInterface<Arg>
	{
		using ArgsTuple = Arg;

		using FnIO = std::tuple_element_t<0, ArgsTuple>;
		using FnIOResult = std::invoke_result_t<FnIO>;
		using FnIOValue = unwrap_async_t<FnIOResult>;
		static constexpr bool kIsFnIOAsync = unwrap_async_v<FnIOResult>;

		using ValueIO = std::tuple_element_t<1, ArgsTuple>;
		using ValueIOResult = std::invoke_result_t<ValueIO>;
		using ValueIOValue = unwrap_async_t<ValueIOResult>;
		static constexpr bool kIsValueIOAsync = unwrap_async_v<ValueIOResult>;

		using IOResultType = std::invoke_result_t<FnIOValue, ValueIOValue>;

		static constexpr auto fn = []([[maybe_unused]] auto fn,
									  ArgsTuple args_tuple) -> lf::task<IOResultType>
		{
			auto const [func_io, value_io] = args_tuple;

			lf::eventually<FnIOValue> func;

			if constexpr (kIsFnIOAsync)
			{
				auto async_fn = func_io();

				co_await lf::fork[&func, async_fn.fn](std::move(async_fn.arg));
			}
			else
			{
				co_await lf::fork[&func, lf::lift](func_io);
			}

			lf::eventually<ValueIOValue> value;

			if constexpr (kIsValueIOAsync)
			{
				auto async_fn = value_io();

				co_await lf::call[&value, async_fn.fn](std::move(async_fn.arg));
			}
			else
			{
				co_await lf::call[&value, lf::lift](value_io);
			}

			co_await lf::join;

			co_return (*func)(std::move(*value));
		};
	};
	template <class Arg>
	async_function_t(Arg) -> async_function_t<Arg>;
};
}  // namespace boost::hana

namespace vulkandemo::monad
{
namespace io
{

constexpr auto pure(auto && value)
{
	return boost::hana::lift<io_tag_t>(FW(value));
}

struct traverse_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(std::ranges::range auto && values, auto const & kleisli)
		{
			using ValueRange = std::decay_t<decltype(values)>;
			using ValueElem = ValueRange::value_type;

			static_assert(
				!detail::specialisation_of<ValueElem, IO>,
				"traverse expects a range of values, not a IOs");

			using IOElem = decltype(kleisli(std::declval<ValueElem>()));
			using IORange = detail::Unspecialise<ValueRange>::template Specialise<IOElem>;

			auto ios = FW(values) |
				std::views::transform([&](auto && elem) { return kleisli(FW(elem)); }) |
				ranges::to<IORange>();
			return sequence(std::move(ios));
		}

		template <class ElementLifter>
		struct with_kleisli_t
		{
			ElementLifter kliesli;
			constexpr auto operator()(this auto && self, std::ranges::range auto && values)
			{
				return io_factory_t{}(FW(values), FW(self).kliesli);
			}
		};
	};
};

struct filter_t
{
	struct io_factory_t
	{
		static constexpr auto operator()(auto && values, auto && kleisli)
		{
			using Rng = std::decay_t<decltype(values)>;
			using Elem = Rng::value_type;
			using IOElem = std::decay_t<decltype(kleisli(std::declval<Elem>()))>;
			using IOElemResult = std::invoke_result_t<IOElem>;
			using IOElemValue = detail::unwrap_async_t<IOElemResult>;
			static_assert(
				std::is_same_v<IOElemValue, std::optional<Elem>>,
				"filter expects a kleisli that returns an IO whose effect is a std::optional of "
				"the input element");

			Rng filterable_values = values;
			return traverse_t::io_factory_t{}(FW(values), FW(kleisli))
				.fmap(hof::transform_maybes_to_values_t{});
		}

		template <class ElementLifter>
		struct with_kleisli_t
		{
			ElementLifter kleisli;
			constexpr auto operator()(this auto && self, std::ranges::range auto && values)
			{
				return io_factory_t{}(FW(values), FW(self).kleisli);
			}
		};
	};
};

template <Action Act>
struct IO
{
	Act action;
	using Ret = decltype(action());

	decltype(auto) operator()(this auto && self)
	{
		return FW(self).action();
	}

	[[nodiscard]] auto bind(this auto && self, Lifter<IO> auto && lifter)
	{
		return boost::hana::chain(FW(self), FW(lifter));
	}

	[[nodiscard]] auto fmap(this auto && self, Transformer auto && transformer)
	{
		return boost::hana::transform(FW(self), FW(transformer));
	}

	[[nodiscard]] auto traverse(auto && element_lifter) const requires std::ranges::range<Ret>
	{
		return bind(traverse_t::io_factory_t::with_kleisli_t{FW(element_lifter)});
	}

	[[nodiscard]] auto filter(LifterFromTo<typename Ret::value_type, bool> auto && element_lifter)
		const requires std::ranges::range<Ret>
	{
		// Note: a good reason to eschew lambdas is so that we can have ranges of IOs - i.e.
		// where the action type is homogenous, so the IO type as a whole is the same for all
		// elements.
		return bind(filter_t::io_factory_t::with_kleisli_t{element_lifter});
	}
};

// template <typename T>
// IO(T &&) -> IO<IOTask<std::decay_t<T>>>;
//
// template <typename T>
// requires requires (T t) { lf::call[t](); }
// IO(T &&) -> IO<std::decay_t<T>>;

template <typename Tuple>
struct appender_t
{
	Tuple vals;
	constexpr auto operator()(this auto && self, auto && value_to_append)
	{
		using boost::hana::append;
		return append(FW(self).vals, FW(value_to_append));
	}
};
auto sequence(detail::specialisation_of<IO> auto &&... ms)
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
			// Applicative - unwrap two IOs, the first yielding a function and the second
			// yielding a value, then call the function with the value in a new IO. In this
			// case, append the result of an io to a tuple.
			return ap(
				// Transform IO result from a tuple of values to a function that appends a value
				// to the (captured) tuple and returns the new tuple.
				acc.fmap([](auto && values) { return appender_t{FW(values)}; }),
				FW(iom));
			;
		});
}

struct sequence_t
{
	struct io_factory_t
	{

		template <class Arg>
		struct async_function_t : detail::AsyncFunctorInterface<Arg>
		{
			using RngOfIOs = Arg;
			using IOElem = RngOfIOs::value_type;
			using IOElemResult = std::invoke_result_t<IOElem>;
			using IOElemValue = detail::unwrap_async_t<IOElemResult>;
			using Rng = detail::Unspecialise<RngOfIOs>::template Specialise<IOElemValue>;

			static_assert(!detail::specialisation_of<IOElemValue, IO>, "IO effect/return is IO");

			using IOResultType = Rng;

			static constexpr auto fn = []([[maybe_unused]] auto fn,
										  RngOfIOs rng_of_ios) -> lf::task<Rng>
			{
				static constexpr bool kIsIOAsync = detail::unwrap_async_v<IOElemResult>;

				Rng outputs;
				outputs.resize(rng_of_ios.size());

				for (std::size_t idx = 0; idx < rng_of_ios.size(); ++idx)
				{
					auto const & iom = rng_of_ios[idx];

					if constexpr (kIsIOAsync)
					{
						auto async_fn = iom();
						co_await lf::fork[&outputs[idx], async_fn.fn](std::move(async_fn).arg);
					}
					else
					{
						co_await lf::fork[&outputs[idx], lf::lift](iom);
					}
				}
				co_await lf::join;

				co_return outputs;
			};
		};
		template <std::ranges::range RngOfIOs>
		async_function_t(RngOfIOs) -> async_function_t<RngOfIOs>;

		template <std::ranges::range RngOfIOs>
		struct action_t
		{
			RngOfIOs rng_of_ios;

			constexpr auto operator()(this auto && self)
			{
				return async_function_t{FW(self).rng_of_ios};
			}
		};

		constexpr auto operator()(std::ranges::range auto && rng_of_ios) const
		{
			return IO{action_t{FW(rng_of_ios)}};
		}
	};
};

auto sequence(std::ranges::range auto && rng)
{
	return sequence_t::io_factory_t{}(FW(rng));
}

auto fmap(auto &&... ms_and_transformer)
{
	return detail::rotate_right(
		[](auto && transformer, detail::specialisation_of<IO> auto &&... ms)
		{ return sequence(FW(ms)...).fmap(FW(transformer)); },
		FW(ms_and_transformer)...);
}
decltype(auto) operator>>(detail::specialisation_of<IO> auto && lhs, auto && rhs)
{
	return FW(lhs).bind(FW(rhs));
}
decltype(auto) operator>>(
	detail::specialisation_of<IO> auto && lhs,
	detail::specialisation_of<detail::Collect> auto && rhs)
{
	return detail::Collect{std::tuple_cat(std::tuple{FW(lhs)}, FW(rhs).args)};
}
decltype(auto) operator>>(
	detail::specialisation_of<IO> auto && lhs, detail::specialisation_of<IO> auto && rhs)
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
struct StateIO;

struct modify_t
{
	struct io_factory_t
	{
		template <class State, class Value, class Fn>
		struct action_t
		{
			State state;
			Value value;
			Fn fn;

			constexpr auto operator()() const
			{
				return std::pair{value, fn(value, state)};
			}
		};

		constexpr auto operator()(auto && state, auto && value, auto && fn) const
		{
			return io::IO{action_t{FW(state), FW(value), FW(fn)}};
		};
	};

	struct stateio_factory_t
	{
		template <class Value, class Fn>
		struct action_t
		{
			Value value;
			Fn fn;
			constexpr auto operator()(auto && state) const
			{
				return io_factory_t{}(FW(state), value, fn);
			}
		};

		constexpr auto operator()(auto && value, auto && fn) const
		{
			return StateIO{action_t{FW(value), FW(fn)}};
		}

		template <class Fn>
		struct with_mutator_t
		{
			Fn fn;
			constexpr auto operator()(auto && value) const
			{
				return stateio_factory_t{}(FW(value), fn);
			}
		};
	};
};

template <typename Act>
struct StateIO
{
	Act action;
	// using Ret = typename detail::FnTraits<Act>::return_value;

	decltype(auto) operator()(this auto && self, auto && state)
	{
		// Must return an IO monad that itself returns a pair.
		static_assert(
			detail::specialisation_of<decltype(action(state)), io::IO>,
			// && detail::specialisation_of<decltype(action(state)()), std::pair>,
			"StateIO action must return IO<pair<value, state>>");

		return FW(self).action(FW(state));
	}

	auto bind(this auto && self, auto && lifter)
	{
		return boost::hana::chain(FW(self), FW(lifter));
	}

	auto fmap(this auto && self, auto && transformer)
	{
		return boost::hana::transform(FW(self), FW(transformer));
	}

	auto then(this auto && self, detail::specialisation_of<StateIO> auto && stateiom)
	{
		return FW(self).bind([stateiom = FW(stateiom)]([[maybe_unused]] auto &&... unused)
							 { return stateiom; });
	}

	auto store(this auto && self, auto && fn)
	{
		return FW(self).bind(modify_t::stateio_factory_t::with_mutator_t{FW(fn)});
	}
};
}  // namespace stateio
}  // namespace vulkandemo::monad

namespace boost::hana
{
namespace io = vulkandemo::monad::io;
namespace stateio = vulkandemo::monad::stateio;
using vulkandemo::monad::detail::specialisation_of;
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
	constexpr static auto apply(auto && stateiom, auto && lifter)
	{
		return stateio::StateIO{stateio_action_t{FW(stateiom), FW(lifter)}};
	}

	template <class WrappedStateIO, class StateIOLifter>
	struct stateio_action_t
	{
		WrappedStateIO stateiom;
		StateIOLifter lifter;
		constexpr auto operator()(this auto && self, auto && state)
		{
			// NOLINTNEXTLINE(bugprone-use-after-move)
			return FW(self).stateiom(FW(state)).bind(io_lifter_t{FW(self).lifter});
		}
	};

	template <class StateIOLifter>
	struct io_lifter_t
	{
		StateIOLifter lifter;
		constexpr auto operator()(auto value_and_state) const
		{
			if constexpr (requires { lifter(value_and_state.first); })
			{
				auto new_stateio = lifter(std::move(value_and_state.first));
				static_assert(
					specialisation_of<decltype(new_stateio), stateio::StateIO>,
					"StateIO lifter must return a StateIO");
				auto new_io = std::move(new_stateio)(std::move(value_and_state.second));
				static_assert(
					specialisation_of<decltype(new_io), io::IO>,
					"StateIO action must return an IO");
				return new_io;
			}
			else if constexpr (tuple_like<decltype(value_and_state.first)> && requires {
								   std::apply(lifter, value_and_state.first);
							   })
			{
				auto new_stateio = std::apply(lifter, std::move(value_and_state.first));
				static_assert(
					specialisation_of<decltype(new_stateio), stateio::StateIO>,
					"StateIO lifter must return a StateIO");
				auto new_io = std::move(new_stateio)(std::move(value_and_state.second));
				static_assert(
					specialisation_of<decltype(new_io), io::IO>,
					"StateIO action must return an IO");
				return new_io;
			}
			else
			{
				static_assert(false, "StateIO lifter not compatible with value.");
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

		constexpr auto operator()(this auto && self, auto const state)
		{
			return FW(self).stateiom(state).fmap(io_transformer_t{FW(self).transformer, state});
		}
	};

	template <class State, class ValueTransformer>
	struct io_transformer_t
	{
		ValueTransformer transformer;
		State state;

		constexpr auto operator()(this auto && self, auto && value_and_state)
		{
			if constexpr (requires { self.transformer(value_and_state.first); })
			{
				return std::pair{
					FW(self).transformer(FW(value_and_state).first), FW(value_and_state).second};
			}
			else if constexpr (requires { std::apply(self.transformer, value_and_state.first); })
			{
				return std::pair{
					std::apply(FW(self).transformer, FW(value_and_state).first),
					FW(value_and_state).second};
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

		constexpr auto operator()(this auto && self, auto && fn)
		{
			return FW(self).value_stateio.bind(value_lifter_t{FW(fn)});
		}
	};

	template <class Fn>
	struct value_lifter_t
	{
		Fn fn;
		constexpr auto operator()(this auto && self, auto && value)
		{
			return lift<stateio::stateio_tag_t>(FW(self).fn(FW(value)));
		}
	};
};
}  // namespace boost::hana

namespace vulkandemo::monad
{
namespace stateio
{

struct get_state_t
{
	struct io_factory_t
	{
		template <class State>
		struct action_t
		{
			State state;
			constexpr auto operator()() const
			{
				return std::pair{state, state};
			}
		};

		constexpr auto operator()(auto && state) const
		{
			return io::IO{action_t{FW(state)}};
		}
	};

	struct stateio_factory_t
	{
		struct action_t
		{
			constexpr auto operator()(auto && state) const
			{
				return io_factory_t{}(FW(state));
			}
		};

		constexpr auto operator()() const
		{
			return StateIO{action_t{}};
		}
	};
};

constexpr auto get_state()
{
	return get_state_t::stateio_factory_t{}();
}

constexpr auto pure(auto && value)
{
	return boost::hana::lift<stateio_tag_t>(FW(value));
}

constexpr auto lift(detail::specialisation_of<io::IO> auto && iom)
{
	return boost::hana::lift<stateio_tag_t>(FW(iom));
}

auto sequence(detail::specialisation_of<StateIO> auto &&... ms)
{
	using boost::hana::ap;
	using boost::hana::append;
	using boost::hana::fold_left;

	return fold_left(
		std::tuple{FW(ms)...},
		pure(std::tuple{}),
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
