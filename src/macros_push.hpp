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
