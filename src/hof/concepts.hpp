/**
 * Template and concept helpers unavailable elsewhere.
 */
#pragma once
#include <concepts>
#include <type_traits>

#include <immer/detail/rbts/bits.hpp>
#include <immer/vector.hpp>

namespace vulkandemo::hof
{
template <typename Container>
struct unspecialise_t;

template <template <typename...> class Container, typename... OldArgs>
struct unspecialise_t<Container<OldArgs...>>
{
	template <typename... Args>
	using specialise_t = Container<Args...>;
};

template <
	typename T,
	typename Policy,
	immer::detail::rbts::bits_t B,
	immer::detail::rbts::bits_t BL>
struct unspecialise_t<immer::vector<T, Policy, B, BL>>
{
	template <typename NewT>
	using specialise_t = immer::vector<NewT, Policy, B, BL>;
};

namespace detail
{
template <template <typename...> class Template, typename... Args>
void noop(Template<Args...> /*unused*/)
{
}
}  // namespace detail

template <class T, template <typename...> class Template>
concept specialisation_of = requires(T obj)
{
	detail::noop<Template>(obj);
};

namespace detail
{
template <typename, typename>
struct is_applicable : std::false_type
{
};
template <typename Func, template <typename...> typename Tuple, typename... Args>
struct is_applicable<Func, Tuple<Args...>> : std::is_invocable<Func, Args...>
{
};
}  // namespace detail

template <class F, class T>
concept applicable_with = detail::is_applicable<F, T>::value;

namespace detail
{
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
}  // namespace detail

template <class F, class T>
struct invoke_or_apply_result : detail::apply_result<F, T>
{
};

template <class F, class T>
requires std::invocable<F, T> struct invoke_or_apply_result<F, T> : std::invoke_result<F, T>
{
};

template <class F, class T>
using invoke_or_apply_result_t = invoke_or_apply_result<F, T>::type;
}  // namespace vulkandemo::hof