#pragma once

#include <boost/hana/fwd/ap.hpp>
#include <boost/hana/fwd/chain.hpp>
#include <boost/hana/fwd/core.hpp>
#include <boost/hana/fwd/lift.hpp>
#include <boost/hana/fwd/transform.hpp>

#include <libfork/algorithm/lift.hpp>
#include <libfork/core/control_flow.hpp>
#include <libfork/core/eventually.hpp>
#include <libfork/core/just.hpp>

#include "../detail.hpp"
#include "./fwd.hpp"

#include "../../macros_push.hpp"

namespace boost::hana
{
namespace io = vulkandemo::monad::io;
using vulkandemo::monad::detail::applicable_with;
using vulkandemo::monad::detail::AsyncFunctorInterface;
using vulkandemo::monad::detail::invoke_or_apply_result_t;
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

#include "../../macros_pop.hpp"
