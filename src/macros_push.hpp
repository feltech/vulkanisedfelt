// ReSharper disable CppMissingIncludeGuard
// SPDX-License-Identifier: MIT
// Copyright 2024-2026 David Feltell
#pragma push_macro("VK_CHECK")
#ifdef VK_CHECK
#undef VK_CHECK
#endif

#define VK_CHECK(func, msg)                                                                \
	do                                                                                     \
	{ /* NOLINT(cppcoreguidelines-avoid-do-while)  */                                      \
		if (const VkResult result = func; result != VK_SUCCESS)                            \
		{                                                                                  \
			throw std::runtime_error{std::format("{}: {}", msg, string_VkResult(result))}; \
		}                                                                                  \
	} while (false)

#pragma push_macro("FW")
#ifdef FW
#undef FW
#endif
#define FW(a) std::forward<decltype(a)>(a)

// TODO(DF): deprecated, do not use
#pragma push_macro("AUTO")
#ifdef AUTO
#undef AUTO
#endif
#define AUTO(T) std::convertible_to<T> auto &&

#pragma push_macro("OVER")
#ifdef OVER
#undef OVER
#endif
// Duplicated (with modification) from `LIFT` in https://github.com/rollbear/lift
#define OVER(f)                                                             \
	OVER_ba7b8453f262e429575e23dcb2192b33_1(                                \
		a_ba7b8453f262e429575e23dcb2192b33_1,                               \
		(f)(::std::forward<decltype(a_ba7b8453f262e429575e23dcb2192b33_1)>( \
			a_ba7b8453f262e429575e23dcb2192b33_1)...))

#define OVER_ba7b8453f262e429575e23dcb2192b33_1(a, f_of_a) \
	[&](auto &&... a) noexcept(noexcept(f_of_a)) -> decltype(f_of_a) { return f_of_a; }
